
#pragma once

/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#include <stddef.h>
#include "avisynth.h"

static const char* SHAREDMEMORYCLIENT_AVS_PARAMS = "s[port]i[clip_index]i";

typedef struct _SHAREDMEMORYCLIENT_RAW_ARGS
{
    AVSValue dummy, port, clip_index;
} SHAREDMEMORYCLIENT_RAW_ARGS;

#define SHAREDMEMORYCLIENT_ARG_INDEX(name) (offsetof(SHAREDMEMORYCLIENT_RAW_ARGS, name) / sizeof(AVSValue))

#define SHAREDMEMORYCLIENT_ARG(name) args[SHAREDMEMORYCLIENT_ARG_INDEX(name)]

class SharedMemoryClient_parameter_storage_t
{
protected:

    int _port; 
    int _clip_index; 

public:

    SharedMemoryClient_parameter_storage_t(const SharedMemoryClient_parameter_storage_t& o)
    {
        _port = o._port; 
        _clip_index = o._clip_index; 
    }

    SharedMemoryClient_parameter_storage_t( int port, int clip_index )
    {
        _port = port; 
        _clip_index = clip_index; 
    }

    SharedMemoryClient_parameter_storage_t( AVSValue args )
    {
        _port = SHAREDMEMORYCLIENT_ARG(port).AsInt(22050);
        _clip_index = SHAREDMEMORYCLIENT_ARG(clip_index).AsInt(0);
    }
};

#define SHAREDMEMORYCLIENT_CREATE_CLASS(klass) new klass( dummy, SharedMemoryClient_parameter_storage_t( port, clip_index ) )

#ifdef SHAREDMEMORYCLIENT_SIMPLE_MACRO_NAME

#ifdef SIMPLE_MACRO_NAME
#error Simple macro name has already been defined for SIMPLE_MACRO_NAME
#endif

#define SIMPLE_MACRO_NAME SharedMemoryClient

#define ARG SHAREDMEMORYCLIENT_ARG

#define CREATE_CLASS SHAREDMEMORYCLIENT_CREATE_CLASS

#endif

#ifdef SHAREDMEMORYCLIENT_IMPLEMENTATION

AVSValue __cdecl Create_SharedMemoryClient(AVSValue args, void* user_data, IScriptEnvironment* env);

void Register_SharedMemoryClient(IScriptEnvironment* env, void* user_data=NULL)
{
    env->AddFunction("MPP_SharedMemoryClient", 
        SHAREDMEMORYCLIENT_AVS_PARAMS,
        Create_SharedMemoryClient,
        user_data);
}

#else

void Register_SharedMemoryClient(IScriptEnvironment* env, void* user_data=NULL);

#endif

