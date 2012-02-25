#include "stdafx.h"
#define SHAREDMEMORYCLIENT_SIMPLE_MACRO_NAME
#define SHAREDMEMORYCLIENT_IMPLEMENTATION
#include "SharedMemoryClient.h"
#include "SafeEnv.h"
#include "utils.h"

#include <assert.h>
#include <stdexcept>

using namespace std;


AVSValue __cdecl Create_SharedMemoryClient(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    try
    {
        return new SharedMemoryClient(SharedMemoryClient_parameter_storage_t(args), env);
    } catch (AvisynthError&) {
        // no need to process it here
        throw;
    } catch (std::exception& e) {
        env->ThrowError("SharedMemoryClient: %s", e.what());
    } catch (...) {
        env->ThrowError("SharedMemoryClient: Unknown exception");
    }
}

SharedMemoryClient::SharedMemoryClient(SharedMemoryClient_parameter_storage_t& o, IScriptEnvironment* env) :
    SharedMemoryClient_parameter_storage_t(o),
    _manager(get_shared_memory_key("LOCAL", _port)),
    _vi(_manager.header->clips[_clip_index].vi)
{
    if (_manager.header->object_state.shutdown)
    {
        env->ThrowError("The server has been shut down");
    }
    // we don't support audio
    _vi.audio_samples_per_second = 0;
}

PVideoFrame SharedMemoryClient::create_frame(int response_index, IScriptEnvironment* env)
{
    auto& clip = _manager.header->clips[_clip_index];
    const unsigned char* buffer = (const unsigned char*)_manager.header + clip.frame_buffer_offset[response_index];
    PVideoFrame frame = SafeNewVideoFrame(env, _vi);
    copy_plane(frame, buffer, clip.frame_pitch, _vi, PLANAR_Y);
    if (_vi.IsPlanar() && !_vi.IsY8())
    {
        copy_plane(frame, buffer + clip.frame_offset_u, clip.frame_pitch_uv, _vi, PLANAR_U);
        copy_plane(frame, buffer + clip.frame_offset_v, clip.frame_pitch_uv, _vi, PLANAR_V);
    }
    return frame;
}

PVideoFrame SharedMemoryClient::GetFrame(int n, IScriptEnvironment* env)
{
    SharedMemorySourceClientLockAcquire lock(_manager);
    if (_manager.header->object_state.shutdown)
    {
        _manager.request_cond->signal.signal_all();
        env->ThrowError("SharedMemoryClient: The server has been shut down.");
    }
    volatile auto& request = _manager.header->request;
    int response_index = get_response_index(n);
    volatile auto& resp = _manager.header->clips[_clip_index].frame_response[response_index];
    CondVar& cond = *_manager.sync_groups[_clip_index]->response_conds[response_index];
    if (cond.lock.try_lock(SPIN_LOCK_UNIT * 5))
    {
        SpinLockContext<> ctx(cond.lock);
        if (_manager.header->object_state.shutdown)
        {
            _manager.request_cond->signal.signal_all();
            env->ThrowError("SharedMemoryClient: The server has been shut down.");
        }
        if (resp.frame_number == n)
        {
            // prefetch hit
            // don't change client count here since we didn't requested it
            return create_frame(response_index, env);
        }
    }
    while (true)
    {
        {
            CondVarLockAcquire cvla(*_manager.request_cond, CondVarLockAcquire::LOCK_SHORT);
            if (_manager.header->object_state.shutdown)
            {
                _manager.request_cond->signal.signal_all();
                env->ThrowError("SharedMemoryClient: The server has been shut down.");
            }
            cvla.signal_after_unlock = true;
            if (request.request_type == REQ_EMPTY)
            {
                request.request_type = REQ_GETFRAME;
                request.clip_index = _clip_index;
                request.frame_number = n;
                break;
            }
        }
        _manager.request_cond->signal.wait_on_this_side(INFINITE);
    }
    while (true)
    {
        bool wait_for_other_client = false;
        {
            CondVarLockAcquire cvla(cond, CondVarLockAcquire::LOCK_LONG);
            if (_manager.header->object_state.shutdown)
            {
                _manager.request_cond->signal.signal_all();
                env->ThrowError("SharedMemoryClient: The server has been shut down.");
            }
            if (resp.frame_number != n)
            {
                if (resp.requested_client_count == 0)
                {
                    // no one needs this frame now, let server fetch new frame
                    cvla.signal_after_unlock = true;
                } else {
                    // another thread needs this frame, ensure it won't be dead-locked
                    wait_for_other_client = true;
                }
            } else {
                PVideoFrame frame = create_frame(response_index, env);
                _InterlockedDecrement(&resp.requested_client_count);
                cvla.signal_after_unlock = true;
                return frame;
            }
        }
        if (wait_for_other_client)
        {
            cond.signal.stay_on_this_side();
            Sleep(1);
        }
    }
}

bool SharedMemoryClient::GetParity(int n)
{
    SharedMemorySourceClientLockAcquire lock(_manager);
    volatile auto& request = _manager.header->request;
    volatile parity_response_t& result_reference = _manager.header->clips[_clip_index].parity_response[get_response_index(n)];
    while (true)
    {
        {
            CondVarLockAcquire cvla(*_manager.request_cond, CondVarLockAcquire::LOCK_SHORT);
            if (_manager.header->object_state.shutdown)
            {
                _manager.request_cond->signal.signal_all();
                return false;
            }
            cvla.signal_after_unlock = true;
            if (request.request_type == REQ_EMPTY)
            {
                request.request_type = REQ_GETPARITY;
                request.clip_index = _clip_index;
                request.frame_number = n;
                break;
            }
        }
        _manager.request_cond->signal.wait_on_this_side(INFINITE);
    }
    parity_response_t result;
    while (true)
    {
        if (_manager.header->object_state.shutdown)
        {
            _manager.request_cond->signal.signal_all();
            return false;
        }
        result.value = _InterlockedCompareExchange64(&result_reference.value, 0, 0);
        if (result.frame_number != n || (result.parity & 0x80) == 0)
        {
            _manager.parity_signal->wait_on_this_side(INFINITE);
            continue;
        }
        while (true)
        {
            auto old_value = result.value;

            auto new_result = result;
            new_result.sequence_number++;
            new_result.requested_client_count--;
            result.value = _InterlockedCompareExchange64(&result_reference.value, new_result.value, old_value);
            assert(result.frame_number == n);
            assert((result.parity & 0x80) != 0);
            if (result.value == old_value)
            {
                return !!(result.parity & 0x1);
            }
        }
    }
}

const VideoInfo& SharedMemoryClient::GetVideoInfo()
{
    return _vi;
}

void SharedMemoryClient::SetCacheHints(int cachehints,int frame_range)
{
    // we don't handle caching here
}

void SharedMemoryClient::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
    // this shouldn't be called since we disabled audio
    assert(false);
}