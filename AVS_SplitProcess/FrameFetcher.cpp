#include "stdafx.h"
#include "FrameFetcher.h"

#include <process.h>
#include <assert.h>

using namespace std;

unsigned __stdcall FrameFetcher::thread_stub(void* fetcher)
{
    return ((FrameFetcher*)fetcher)->thread_proc();
}

unsigned FrameFetcher::thread_proc()
{
    int fetch_info_version;

    { // lock start
        CSLockAcquire lock(_lock);
        fetch_info_version = _fetch_info.version;
    } // lock end

    while (!_shutdown)
    {
        ClipInfo* clip_to_fetch = NULL;
        int requested_frame = -1;
        bool is_requested_fetch = false;

        { // lock start
            CSLockAcquire lock(_lock);
            if (_fetch_info.version != fetch_info_version)
            {
                clip_to_fetch = &_clips[_fetch_info.clip_index];
                assert(clip_to_fetch->error_msg.empty());

                requested_frame = _fetch_info.frame_number;
                fetch_info_version = _fetch_info.version;
                is_requested_fetch = true;
            } else {
                int min_cached_frames = 0x7fffffff;
                for (size_t i = 0; i < (int)_clips.size(); i++)
                {
                    ClipInfo& clip = _clips[i];
                    int cached_frames = clip.frame_cache.size();
                    if (cached_frames > _fetch_ahead + _cache_behind + 1) 
                    {
                        continue;
                    }
                    if (!clip.error_msg.empty())
                    {
                        continue;
                    }
                    if (cached_frames < min_cached_frames)
                    {
                        clip_to_fetch = &clip;
                        requested_frame = clip.cache_frame_start + cached_frames;
                        min_cached_frames = cached_frames;
                        assert(requested_frame >= 0);
                    }
                }
            }
        } // lock end

        if (!clip_to_fetch)
        {
            SetEvent(_worker_workitem_completed_event.get());
            _worker_waiting_for_work_event.wait(500);
            continue;
        }

        assert(requested_frame >= 0);

        fetch_frame(*clip_to_fetch, requested_frame);

        if (is_requested_fetch)
        {
            CSLockAcquire lock(_lock);
            assert(_fetch_info.is_fetching);
            assert(_fetch_info.version == fetch_info_version);
            _fetch_info.is_fetching = false;
        }

        SetEvent(_worker_workitem_completed_event.get());
        _worker_waiting_for_work_event.wait(10);
    }
    return 0;
}

void FrameFetcher::fetch_frame(ClipInfo& clip, int n)
{
    int fetch_start = -1;

    { // lock start
        CSLockAcquire lock(_lock);
        while (clip.frame_cache.size() > 0 && n - clip.cache_frame_start > _cache_behind)
        {
            clip.frame_cache.pop_front();
            clip.cache_frame_start++;
        }
        if (clip.frame_cache.size() == 0) 
        {
            clip.cache_frame_start = n;
            fetch_start = n;
        } else {
            fetch_start = clip.cache_frame_start + clip.frame_cache.size();
        }
    } // lock end
    
    // sanity check
    assert(fetch_start >= 0);
    assert(n - fetch_start < _cache_behind);

    while (fetch_start <= n)
    {
        PVideoFrame frame = try_get_frame(clip, fetch_start);
        if (!frame)
        {
            return;
        }
        { // lock start
            CSLockAcquire lock(_lock);
            clip.frame_cache.push_back(frame);
        } // lock end
        fetch_start++;
    }
}

PVideoFrame FrameFetcher::try_get_frame(ClipInfo& clip, int n)
{
    assert(clip.error_msg.empty());
    PVideoFrame frame = NULL;
    try
    {
        frame = clip.clip->GetFrame(n, _env);
    } catch (AvisynthError& ex) {
        CSLockAcquire lock(_lock);
        clip.error_msg.assign(ex.msg);
        clip.frame_cache.clear();
        frame = NULL;
    }
    return frame;
}

void wait_and_reset_event(OwnedHandle& event)
{
    event.wait();
    ResetEvent(event.get());
}

PVideoFrame FrameFetcher::GetFrame(int clip_index, int n, IScriptEnvironment* env)
{
    assert(env == _env);
    while (true)
    {
        { // lock start
            CSLockAcquire lock(_lock);
            ClipInfo& clip = _clips[clip_index];
            if (!clip.error_msg.empty())
            {
                env->ThrowError(clip.error_msg.c_str());
            }
            if (n >= clip.cache_frame_start && n < clip.cache_frame_start + (int)clip.frame_cache.size())
            {
                clip.last_requested_frame = n;
                return clip.frame_cache.at(n - clip.cache_frame_start);
            }
            if (!_fetch_info.is_fetching)
            {
                _fetch_info.is_fetching = true;
                _fetch_info.clip_index = clip_index;
                _fetch_info.frame_number = n;
                _fetch_info.version++;
                SetEvent(_worker_waiting_for_work_event.get());
            }
        } // lock end
        wait_and_reset_event(_worker_workitem_completed_event);
    }
}

FrameFetcher::FrameFetcher(PClip clips[], int fetch_ahead, int cache_behind, IScriptEnvironment* env)
    : _fetch_ahead(fetch_ahead),  
      _cache_behind(cache_behind),
      _env(env),
      _clips(),
      _lock(),
      _worker_thread(NULL),
      _worker_workitem_completed_event(CreateEvent(NULL, TRUE, FALSE, NULL)), // needs to be manual reset, since we want all threads waiting on this event to be released when it is signaled
      _worker_waiting_for_work_event(CreateEvent(NULL, FALSE, FALSE, NULL)),
      _shutdown(false)
{
    memset(&_fetch_info, 0, sizeof(_fetch_info));

    if (!_worker_workitem_completed_event.is_valid() || !_worker_waiting_for_work_event.is_valid())
    {
        env->ThrowError("FrameFetcher: Unable to create event.");
    }

    int i = 0;
    while (clips[i])
    {
        _clips.push_back(ClipInfo(clips[i]));
        i++;
    }
    if (_clips.size() == 0)
    {
        env->ThrowError("FrameFetcher: Unexpected exception: No input clip.");
    }
    _worker_thread.replace((HANDLE)_beginthreadex(NULL, 0, FrameFetcher::thread_stub, this, 0, NULL));
    if (!_worker_thread.is_valid())
    {
        env->ThrowError("FrameFetcher: Can't start worker thread.");
    }
}

FrameFetcher::~FrameFetcher()
{
    _shutdown = true;
    SetEvent(_worker_waiting_for_work_event.get());
    _worker_thread.wait();
    _env = NULL;
}