#pragma once

#include "avisynth.h"
#include "Handle.h"
#include "TwoSidedLock.h"
#include "NonCopyableClassBase.h"
#include "SystemChar.h"

#include <string>

static const unsigned int SHARED_MEMORY_SOURCE_SIGNATURE = 0x4d50534d;

typedef enum _request_type_t
{
    REQ_GETFRAME = 1,
    REQ_GETPARITY
} request_type_t;

typedef struct _shared_memory_source_request_t
{
    // filled by server
    int response_object_id;

    // filled by client
    request_type_t request_type;
    int clip_index;
    int frame_number;
} shared_memory_source_request_t;

typedef struct _shared_memory_source_response_t
{
    // set by server, relative to the beginning of the mapped region
    // immutable after initialization
    unsigned int buffer_offset;

    // for debugging
    request_type_t request_type;
    int clip_index;
    int frame_number;

    union 
    {
        struct
        {
            bool parity;
        } response_GetParity;
        struct
        {
            int pitch;
            int pitch_uv;

            // relative to the beginning of buffer
            int offset_u;
            int offset_v;

            // frame data is stored in the buffer specified by buffer_offset
        } response_GetFrame;
    } response_data;
} shared_memory_source_response_t;

typedef struct _shared_memory_source_header_t
{
    unsigned int signature;
    int clip_count;
    int data_buffer_size;

    volatile bool shutdown;
    volatile unsigned client_count;

    shared_memory_source_request_t request;
    shared_memory_source_response_t response[2];

    // immutable after initialization
    VideoInfo vi_array[1];
} shared_memory_source_header_t;

// provide compatibility with TCPDeliver
sys_string get_shared_memory_key(const char* key1, int key2);

class SharedMemorySourceManager : private NonCopyableClassBase
{
public:
    typedef enum _lock_type_t
    {
        request_lock = 0,
        response_0_lock,
        response_1_lock
    } lock_type_t;

    SharedMemorySourceManager(const sys_string key, bool is_server, int clip_count, const VideoInfo vi_array[]);
    ~SharedMemorySourceManager();
    shared_memory_source_header_t* header;

    TwoSidedLock& get_lock_by_type(lock_type_t lock_type);

    void check_data_buffer_integrity(int response_object_id);

    void signal_shutdown();

private:
    void init_server(const SYSCHAR* mapping_name, int clip_count, const VideoInfo vi_array[]);
    void init_client(const SYSCHAR* mapping_name);
    void map_view();

    bool _is_server;

    OwnedHandle _mapping_handle;
    TwoSidedLock _request_lock;
    TwoSidedLock _response_0_lock;
    TwoSidedLock _response_1_lock;
};
