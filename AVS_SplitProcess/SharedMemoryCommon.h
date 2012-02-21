#pragma once

#include "avisynth.h"
#include "Handle.h"
#include "TwoSidedLock.h"
#include "NonCopyableClassBase.h"
#include "SystemChar.h"
#include "SpinLock.h"
#include "CondVar.h"

#include <string>
#include <vector>
#include <memory>

static const unsigned int SHARED_MEMORY_SOURCE_SIGNATURE = 0x4d50534d;

typedef enum _request_type_t
{
    REQ_GETFRAME = 1,
    REQ_GETPARITY,

    REQ_EMPTY = 0x7f
} request_type_t;

typedef struct _shared_memory_source_request_t
{
    request_type_t request_type;
    int clip_index;
    int frame_number;
} shared_memory_source_request_t;



typedef __declspec(align(64)) struct _shared_memory_clip_info_t
{
    // --- immutable after initialization ---
    VideoInfo vi;

    // buffer size of ONE frame
    unsigned int frame_buffer_size;
    // relative to the beginning of the mapped region
    unsigned int frame_buffer_offset[2];

    int frame_pitch;
    int frame_pitch_uv;

    // relative to the beginning of buffer 
    int frame_offset_u;
    int frame_offset_v;
    // --------------------------------------

    // align the volatile part to beginning of a cache line, 
    // so only one cache line is affected when someone changes its content
    // (don't know whether it is necessary, but memory is cheap :P )
    __declspec(align(64))
    volatile struct
    {
        // response index:
        // LSB of the frame number is used as index to the array

        // parity response format:
        // MSB is parity of the requested frame, remaining part is the frame number

        // client sets the corresponding response to 0xFFFFFFFF after locking the request,
        // and poll the value until it is set to a valid value, no more locking needed
        int parity_response[2];

        struct 
        {

            spin_lock_value_type_t lock;

            int frame_number;
            bool is_prefetched_frame;
            int client_read_count;

        } frame_response[2];
    };
} shared_memory_clip_info_t;

typedef struct _shared_memory_source_header_t
{
    unsigned int signature;
    int clip_count;

    volatile bool shutdown;
    volatile unsigned client_count;

    volatile spin_lock_value_type_t request_lock;
    volatile shared_memory_source_request_t request;

    shared_memory_clip_info_t clips[1];
} shared_memory_source_header_t;

class ClipSyncGroup
{
public:
    ClipSyncGroup(const sys_string& name, int clip_index, shared_memory_clip_info_t& clip_info);
    std::vector<std::unique_ptr<CondVar> > response_conds;
};

// provide compatibility with TCPDeliver
sys_string get_shared_memory_key(const char* key1, int key2);

int get_response_index(int frame_number);

class SharedMemorySourceManager : private NonCopyableClassBase
{
public:
    
    SharedMemorySourceManager(const sys_string key, int clip_count, const VideoInfo vi_array[]);
    SharedMemorySourceManager(const sys_string key);
    ~SharedMemorySourceManager();
    shared_memory_source_header_t* header;

    void check_data_buffer_integrity(int clip_index, int response_object_id);

    void signal_shutdown();

    std::unique_ptr<CondVar> request_cond;
    std::vector< std::unique_ptr<ClipSyncGroup> > sync_groups;

private:
    void init_server(const SYSCHAR* mapping_name, int clip_count, const VideoInfo vi_array[]);
    void init_client(const SYSCHAR* mapping_name);
    void init_sync_objects(const sys_string& key, int clip_count);
    void map_view();

    bool _is_server;

    OwnedHandle _mapping_handle;
    
};
