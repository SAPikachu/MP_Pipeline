#pragma once

#include "SharedMemoryServer.def.h"
#include "FrameFetcher.h"
#include "SharedMemoryCommon.h"
#include "Handle.h"

#include <vector>

class SharedMemoryServer : private SharedMemoryServer_parameter_storage_t, public GenericVideoFilter
{
public:
    SharedMemoryServer(const PClip clips[], int clip_count, const VideoInfo vi_array[], SharedMemoryServer_parameter_storage_t& o, IScriptEnvironment* env);
    ~SharedMemoryServer();

    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    bool __stdcall GetParity(int n);


private:
    unsigned thread_proc();
    static unsigned __stdcall thread_stub(void* self);

    bool try_prefetch_frame();

    void copy_frame(PVideoFrame frame, int clip_index, int response_index);
    
    void process_get_parity(const shared_memory_source_request_t& request);
    void process_get_frame(const shared_memory_source_request_t& request);
    
    void initiate_shutdown();
    bool is_shutting_down() const;

    void wait_for_activity();

    IScriptEnvironment* _env;
    FrameFetcher _fetcher;
    SharedMemorySourceManager _manager;
    OwnedHandle _thread_handle;
    std::vector<HANDLE> _activity_wait_handles;
    volatile bool _shutdown;
};