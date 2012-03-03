
#pragma once

/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#include <stddef.h>
#include "avisynth.h"

static const char* SHAREDMEMORYSERVER_AVS_PARAMS = "c[port]i[aux_clips]c*[max_cache_frames]i[cache_behind]i[fetcher_thread_lock_to_cpu]i[server_thread_lock_to_cpu]i";

typedef struct _SHAREDMEMORYSERVER_RAW_ARGS
{
    AVSValue child, port, aux_clips, max_cache_frames, cache_behind, fetcher_thread_lock_to_cpu, server_thread_lock_to_cpu;
} SHAREDMEMORYSERVER_RAW_ARGS;

#define SHAREDMEMORYSERVER_ARG_INDEX(name) (offsetof(SHAREDMEMORYSERVER_RAW_ARGS, name) / sizeof(AVSValue))

#define SHAREDMEMORYSERVER_ARG(name) args[SHAREDMEMORYSERVER_ARG_INDEX(name)]

class SharedMemoryServer_parameter_storage_t
{
protected:

    int _port; 
    int _max_cache_frames; 
    int _cache_behind; 
    int _fetcher_thread_lock_to_cpu; 
    int _server_thread_lock_to_cpu; 

public:

    SharedMemoryServer_parameter_storage_t(const SharedMemoryServer_parameter_storage_t& o)
    {
        _port = o._port; 
        _max_cache_frames = o._max_cache_frames; 
        _cache_behind = o._cache_behind; 
        _fetcher_thread_lock_to_cpu = o._fetcher_thread_lock_to_cpu; 
        _server_thread_lock_to_cpu = o._server_thread_lock_to_cpu; 
    }

    SharedMemoryServer_parameter_storage_t( int port, int max_cache_frames, int cache_behind, int fetcher_thread_lock_to_cpu, int server_thread_lock_to_cpu )
    {
        _port = port; 
        _max_cache_frames = max_cache_frames; 
        _cache_behind = cache_behind; 
        _fetcher_thread_lock_to_cpu = fetcher_thread_lock_to_cpu; 
        _server_thread_lock_to_cpu = server_thread_lock_to_cpu; 
    }

    SharedMemoryServer_parameter_storage_t( AVSValue args )
    {
        _port = SHAREDMEMORYSERVER_ARG(port).AsInt(22050);
        _max_cache_frames = SHAREDMEMORYSERVER_ARG(max_cache_frames).AsInt(1);
        _cache_behind = SHAREDMEMORYSERVER_ARG(cache_behind).AsInt(0);
        _fetcher_thread_lock_to_cpu = SHAREDMEMORYSERVER_ARG(fetcher_thread_lock_to_cpu).AsInt(-1);
        _server_thread_lock_to_cpu = SHAREDMEMORYSERVER_ARG(server_thread_lock_to_cpu).AsInt(-1);
    }
};

#define SHAREDMEMORYSERVER_CREATE_CLASS(klass) new klass( child, aux_clips, SharedMemoryServer_parameter_storage_t( port, max_cache_frames, cache_behind, fetcher_thread_lock_to_cpu, server_thread_lock_to_cpu ) )

#ifdef SHAREDMEMORYSERVER_SIMPLE_MACRO_NAME

#ifdef SIMPLE_MACRO_NAME
#error Simple macro name has already been defined for SIMPLE_MACRO_NAME
#endif

#define SIMPLE_MACRO_NAME SharedMemoryServer

#define ARG SHAREDMEMORYSERVER_ARG

#define CREATE_CLASS SHAREDMEMORYSERVER_CREATE_CLASS

#endif

#ifdef SHAREDMEMORYSERVER_IMPLEMENTATION

AVSValue __cdecl Create_SharedMemoryServer(AVSValue args, void* user_data, IScriptEnvironment* env);

void Register_SharedMemoryServer(IScriptEnvironment* env, void* user_data=NULL)
{
    env->AddFunction("MPP_SharedMemoryServer", 
        SHAREDMEMORYSERVER_AVS_PARAMS,
        Create_SharedMemoryServer,
        user_data);
}

#else

void Register_SharedMemoryServer(IScriptEnvironment* env, void* user_data=NULL);

#endif

