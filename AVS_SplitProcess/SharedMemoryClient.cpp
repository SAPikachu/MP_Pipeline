#include "stdafx.h"
#include "SharedMemoryClient.h"

#include <assert.h>

using namespace std;

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
bool SharedMemoryClient::GetParity(int n)
{
    SharedMemorySourceClientLockAcquire lock(_manager);
    volatile auto& request = _manager.header->request;
    volatile long& result_reference = _manager.header->clips[_clip_index].parity_response[get_response_index(n)];
    while (true)
    {
        _manager.request_cond->lock_short();
        {
            SpinLockContext<> ctx(_manager.request_cond->lock);
            if (_manager.header->object_state.shutdown)
            {
                _manager.request_cond->signal.signal_all();
                return false;
            }
            if (request.request_type == REQ_EMPTY)
            {
                request.request_type = REQ_GETPARITY;
                request.clip_index = _clip_index;
                request.frame_number = n;
                while (_InterlockedCompareExchange(&result_reference, PARITY_WAITING_FOR_RESPONSE, PARITY_RESPONSE_EMPTY) != PARITY_RESPONSE_EMPTY)
                {
                }
                _manager.request_cond->signal.switch_to_other_side();
                break;
            }
        }
        _manager.request_cond->signal.wait_on_this_side(INFINITE);
    }
    while (true)
    {
        long result = result_reference;
        if (result == PARITY_WAITING_FOR_RESPONSE)
        {
            Sleep(0);
            continue;
        }
        assert((result & 0x7fffffff) == n);
        _InterlockedCompareExchange(&result_reference, PARITY_RESPONSE_EMPTY, result);
        return !!(result & 0x80000000);
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