
#pragma once

/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#include <stddef.h>
#include "avisynth.h"

static const char* THUNKEDINTERLEAVE_AVS_PARAMS = "ic+";

typedef struct _THUNKEDINTERLEAVE_RAW_ARGS
{
    AVSValue thunk_size, clips;
} THUNKEDINTERLEAVE_RAW_ARGS;

#define THUNKEDINTERLEAVE_ARG_INDEX(name) (offsetof(THUNKEDINTERLEAVE_RAW_ARGS, name) / sizeof(AVSValue))

#define THUNKEDINTERLEAVE_ARG(name) args[THUNKEDINTERLEAVE_ARG_INDEX(name)]

class ThunkedInterleave_parameter_storage_t
{
protected:

    int _thunk_size; 

public:

    ThunkedInterleave_parameter_storage_t(const ThunkedInterleave_parameter_storage_t& o)
    {
        _thunk_size = o._thunk_size; 
    }

    ThunkedInterleave_parameter_storage_t( int thunk_size )
    {
        _thunk_size = thunk_size; 
    }

    ThunkedInterleave_parameter_storage_t( AVSValue args )
    {
        _thunk_size = THUNKEDINTERLEAVE_ARG(thunk_size).AsInt();
    }
};

#define THUNKEDINTERLEAVE_CREATE_CLASS(klass) new klass( clips, ThunkedInterleave_parameter_storage_t( thunk_size ) )

#ifdef THUNKEDINTERLEAVE_SIMPLE_MACRO_NAME

#ifdef SIMPLE_MACRO_NAME
#error Simple macro name has already been defined for SIMPLE_MACRO_NAME
#endif

#define SIMPLE_MACRO_NAME ThunkedInterleave

#define ARG THUNKEDINTERLEAVE_ARG

#define CREATE_CLASS THUNKEDINTERLEAVE_CREATE_CLASS

#endif

#ifdef THUNKEDINTERLEAVE_IMPLEMENTATION

AVSValue __cdecl Create_ThunkedInterleave(AVSValue args, void* user_data, IScriptEnvironment* env);

void Register_ThunkedInterleave(IScriptEnvironment* env, void* user_data=NULL)
{
    env->AddFunction("ThunkedInterleave", 
        THUNKEDINTERLEAVE_AVS_PARAMS,
        Create_ThunkedInterleave,
        user_data);
}

#else

void Register_ThunkedInterleave(IScriptEnvironment* env, void* user_data=NULL);

#endif

