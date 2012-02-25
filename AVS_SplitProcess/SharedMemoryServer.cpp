#include "stdafx.h"
#define SHAREDMEMORYSERVER_SIMPLE_MACRO_NAME
#define SHAREDMEMORYSERVER_IMPLEMENTATION
#include "SharedMemoryServer.h"
#include "trace.h"
#include "utils.h"
#include <assert.h>
#include <process.h>
#include <exception>
#include <memory>

using namespace std;

AVSValue __cdecl Create_SharedMemoryServer(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    SharedMemoryServer_parameter_storage_t params(args);
    AVSValue aux_clips = ARG(aux_clips);
    assert(aux_clips.IsArray());
    int array_size = aux_clips.ArraySize();
    unique_ptr<PClip[]> clips(new PClip[array_size + 2]);
    clips[0] = ARG(child).AsClip();
    for (int i = 0; i < array_size; i++)
    {
        clips[i + 1] = aux_clips[i].AsClip();
    }
    clips[array_size + 1] = NULL;
    unique_ptr<VideoInfo[]> vi_array(new VideoInfo[array_size + 1]);
    for (int i = 0; i < array_size + 1; i++)
    {
        vi_array[i] = clips[i]->GetVideoInfo();
        // disable audio
        vi_array[i].audio_samples_per_second = 0;
    }
    try
    {
        return new SharedMemoryServer(clips.get(), array_size + 1, vi_array.get(), params, env);
    } catch (AvisynthError&) {
        // no need to process it here
        throw;
    } catch (std::exception& e) {
        env->ThrowError("SharedMemoryServer: %s", e.what());
    } catch (...) {
        env->ThrowError("SharedMemoryServer: Unknown exception");
    }
}

bool SharedMemoryServer::is_shutting_down() const
{
    return _shutdown || _manager.header->object_state.shutdown;
}

unsigned __stdcall SharedMemoryServer::thread_stub(void* self)
{
    return ((SharedMemoryServer*)self)->thread_proc();
}

void trace_avs_error(AvisynthError& e)
{
    TRACE("AvisynthError: %hs", e.msg);
}

void SharedMemoryServer::process_get_parity(shared_memory_source_request_t& request)
{
    TwoSidedLockContext ctx(*_manager.parity_signal);

    int response_index = get_response_index(request.frame_number);
    volatile parity_response_t& result_reference = _manager.header->clips[request.clip_index].parity_response[response_index];
    parity_response_t result;

    char parity = 0;

    while (true)
    {
        if (is_shutting_down())
        {
            return;
        }
        result.value = _InterlockedCompareExchange64(&result_reference.value, 0, 0);
        if (result.frame_number == request.frame_number && (result.parity & 0x80) != 0)
        {
            auto new_result = result;
            new_result.sequence_number++;
            new_result.requested_client_count++;
            if (_InterlockedCompareExchange64(&result_reference.value, new_result.value, result.value) == result.value)
            {
                return;
            } else {
                Sleep(0);
                continue;
            }
        }

        assert(result.requested_client_count >= 0);
        if (result.requested_client_count > 0)
        {
            Sleep(0);
            continue;
        }

        if ((parity & 0x80) == 0)
        {
            parity = 0x80 | (_fetcher.GetParity(request.clip_index, request.frame_number) ? 1 : 0);
        }

        auto new_result = result;
        new_result.sequence_number++;
        new_result.frame_number = request.frame_number;
        new_result.requested_client_count++;
        new_result.parity = parity;
        if (_InterlockedCompareExchange64(&result_reference.value, new_result.value, result.value) == result.value)
        {
            return;
        } else {
            Sleep(0);
            continue;
        }
    }
}

void SharedMemoryServer::process_get_frame(shared_memory_source_request_t& request)
{
    int response_index = get_response_index(request.frame_number);
    assert(response_index >= 0 && response_index < 2);
    CondVar& cond = *_manager.sync_groups[request.clip_index]->response_conds[response_index];
    auto& clip = _manager.header->clips[request.clip_index];
    auto& response = clip.frame_response[response_index];

    // since only this thread can modify the response, don't need locking here
    if (response.frame_number == request.frame_number)
    {
        _InterlockedIncrement(&response.requested_client_count);
        cond.signal.switch_to_other_side();
        return;
    }

    PVideoFrame frame = _fetcher.GetFrame(request.clip_index, request.frame_number, _env);
    if (!frame)
    {
        throw runtime_error("Unable to fetch frame");
    }

    unsigned char* buffer = (unsigned char*)_manager.header + clip.frame_buffer_offset[response_index];
    while (true)
    {
        {
            CondVarLockAcquire cvla(cond, CondVarLockAcquire::LOCK_LONG);
            if (is_shutting_down())
            {
                cond.signal.signal_all();
                return;
            }
            assert(response.requested_client_count >= 0);
            cvla.signal_after_unlock = true;
            if (response.requested_client_count == 0)
            {
                response.frame_number = request.frame_number;
                // note: use Interlocked functions if need to change client count in other place!
                response.requested_client_count = 1;
                copy_plane(buffer, clip.frame_pitch, frame, clip.vi, PLANAR_Y);
                if (clip.vi.IsPlanar() && !clip.vi.IsY8())
                {
                    copy_plane(buffer + clip.frame_offset_u, clip.frame_pitch_uv, frame, clip.vi, PLANAR_U);
                    copy_plane(buffer + clip.frame_offset_v, clip.frame_pitch_uv, frame, clip.vi, PLANAR_V);
                }
                _manager.check_data_buffer_integrity(request.clip_index, response_index);
                break;
            }
        }
        // a client requested a frame, but not fetched yet
        cond.signal.wait_on_this_side(INFINITE);
    }
}

unsigned SharedMemoryServer::thread_proc()
{
    shared_memory_source_request_t request;
    while (!is_shutting_down())
    {
        request.request_type = REQ_EMPTY;

        {
            CondVarLockAcquire cvla(*_manager.request_cond, CondVarLockAcquire::LOCK_SHORT);
            if (is_shutting_down())
            {
                _manager.request_cond->signal.signal_all();
                break;
            }
            cvla.signal_after_unlock = true;
            if (_manager.header->request.request_type != REQ_EMPTY)
            {
                memcpy(&request, (const void*)&(_manager.header->request), sizeof(request));
                _manager.header->request.request_type = REQ_EMPTY; 
            }
        }

        if (request.request_type == REQ_EMPTY)
        {
            // TODO: prefetch frame when there is no request
            _manager.request_cond->signal.wait_on_this_side(INFINITE);
            continue;
        }
        bool success = false;
        try
        {
            switch (request.request_type)
            {
            case REQ_GETFRAME:
                process_get_frame(request);
                break;
            case REQ_GETPARITY:
                process_get_parity(request);
                break;
            default:
                assert(false);
                throw std::invalid_argument("Invalid request.");
            }
            success = true;
        } catch (AvisynthError& e) {
            trace_avs_error(e);
        } catch (runtime_error& e) {
            UNREFERENCED_PARAMETER(e); // eliminate warning in release build
            TRACE("Runtime error while handling request: %hs", e.what());
        } catch (...) {
            TRACE("Unknown error occurred while handling request.");
            assert(false);
            __debugbreak();
        }
        if (!success)
        {
            initiate_shutdown();
            return 1;
        }
    }
    initiate_shutdown();
    return 0;
}

void SharedMemoryServer::initiate_shutdown()
{
    _shutdown = true;
    _manager.signal_shutdown();
}

SharedMemoryServer::SharedMemoryServer(const PClip clips[], int clip_count, const VideoInfo vi_array[], SharedMemoryServer_parameter_storage_t& o, IScriptEnvironment* env):
    SharedMemoryServer_parameter_storage_t(o),
    GenericVideoFilter(clips[0]),
    _thread_handle(NULL),
    _env(env),
    _shutdown(false),
    _fetcher(clips, _max_cache_frames, _cache_behind, env),
    _manager(get_shared_memory_key("LOCAL", _port), clip_count, vi_array)
{
    _thread_handle.replace((HANDLE)_beginthreadex(NULL, 0, thread_stub, this, 0, 0));
    if (!_thread_handle.is_valid())
    {
        env->ThrowError("SharedMemoryServer: Unable to create thread.");
    }
}

SharedMemoryServer::~SharedMemoryServer()
{
    initiate_shutdown();
    while (_thread_handle.wait(10) == WAIT_TIMEOUT)
    {
        initiate_shutdown();
    }
}

PVideoFrame SharedMemoryServer::GetFrame(int n, IScriptEnvironment* env)
{
    return _fetcher.GetFrame(0, n, env);
}

bool SharedMemoryServer::GetParity(int n)
{
    return _fetcher.GetParity(0, n);
}