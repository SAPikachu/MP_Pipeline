
#pragma once

/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#include <stddef.h>
#include "avisynth.h"

static const char* MP_PIPELINE_AVS_PARAMS = "s[always_kill_children]b";

typedef struct _MP_PIPELINE_RAW_ARGS
{
    AVSValue script, always_kill_children;
} MP_PIPELINE_RAW_ARGS;

#define MP_PIPELINE_ARG_INDEX(name) (offsetof(MP_PIPELINE_RAW_ARGS, name) / sizeof(AVSValue))

#define MP_PIPELINE_ARG(name) args[MP_PIPELINE_ARG_INDEX(name)]

class MP_Pipeline_parameter_storage_t
{
protected:

    const char* _script; 
    bool _always_kill_children; 

public:

    MP_Pipeline_parameter_storage_t(const MP_Pipeline_parameter_storage_t& o)
    {
        _script = o._script; 
        _always_kill_children = o._always_kill_children; 
    }

    MP_Pipeline_parameter_storage_t( const char* script, bool always_kill_children )
    {
        _script = script; 
        _always_kill_children = always_kill_children; 
    }

    MP_Pipeline_parameter_storage_t( AVSValue args )
    {
        _script = MP_PIPELINE_ARG(script).AsString();
        _always_kill_children = MP_PIPELINE_ARG(always_kill_children).AsBool(false);
    }
};

#define MP_PIPELINE_CREATE_CLASS(klass) new klass( MP_Pipeline_parameter_storage_t( script, always_kill_children ) )

#ifdef MP_PIPELINE_SIMPLE_MACRO_NAME

#ifdef SIMPLE_MACRO_NAME
#error Simple macro name has already been defined for SIMPLE_MACRO_NAME
#endif

#define SIMPLE_MACRO_NAME MP_Pipeline

#define ARG MP_PIPELINE_ARG

#define CREATE_CLASS MP_PIPELINE_CREATE_CLASS

#endif

#ifdef MP_PIPELINE_IMPLEMENTATION

AVSValue __cdecl Create_MP_Pipeline(AVSValue args, void* user_data, IScriptEnvironment* env);

void Register_MP_Pipeline(IScriptEnvironment* env, void* user_data=NULL)
{
    env->AddFunction("MP_Pipeline", 
        MP_PIPELINE_AVS_PARAMS,
        Create_MP_Pipeline,
        user_data);
}

#else

void Register_MP_Pipeline(IScriptEnvironment* env, void* user_data=NULL);

#endif

