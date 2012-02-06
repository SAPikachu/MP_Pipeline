#pragma once

#include "avisynth.h"
#include "Lock.h"
#include "Handle.h"

#include <deque>
#include <vector>
#include <string>

class ClipInfo
{
public:
    ClipInfo(PClip clip) : 
      clip(clip), 
      frame_cache(), 
      cache_frame_start(0), 
      last_requested_frame(0),
      error_msg()
    {};

    PClip clip;
    std::deque<PVideoFrame> frame_cache;
    int cache_frame_start;
    int last_requested_frame;
    std::string error_msg;
};

typedef struct _FetchInfo
{
    int version;
    int clip_index;
    int frame_number;
    bool is_fetching;
} FetchInfo;

class FrameFetcher
{
public:
    FrameFetcher(PClip clips[], int max_cache_frames, int cache_behind, IScriptEnvironment* env);
    ~FrameFetcher();

    PVideoFrame GetFrame(int clip_index, int n, IScriptEnvironment* env);

protected:
    CriticalSectionLock _lock;

private:
    unsigned thread_proc();
    static unsigned __stdcall thread_stub(void* fetcher);
    
    void fetch_frame(ClipInfo& clip, int n);

    PVideoFrame try_get_frame(ClipInfo& clip, int n);

    bool _shutdown;
    int _max_cache_frames;
    int _cache_behind;
    std::vector<ClipInfo> _clips;

    IScriptEnvironment* _env;

    FetchInfo _fetch_info;

    OwnedHandle _worker_thread;
    OwnedHandle _worker_workitem_completed_event;
    OwnedHandle _worker_waiting_for_work_event;
};