
#pragma once

/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#include <stddef.h>
#include "avisynth.h"

static const char* SELECTTHUNKEVERY_AVS_PARAMS = "cii[first_chunk_index]i";

typedef struct _SELECTTHUNKEVERY_RAW_ARGS
{
    AVSValue child, thunk_size, every, first_chunk_index;
} SELECTTHUNKEVERY_RAW_ARGS;

#define SELECTTHUNKEVERY_ARG_INDEX(name) (offsetof(SELECTTHUNKEVERY_RAW_ARGS, name) / sizeof(AVSValue))

#define SELECTTHUNKEVERY_ARG(name) args[SELECTTHUNKEVERY_ARG_INDEX(name)]

class SelectThunkEvery_parameter_storage_t
{
protected:

    int _thunk_size; 
    int _every; 
    int _first_chunk_index; 

public:

    SelectThunkEvery_parameter_storage_t(const SelectThunkEvery_parameter_storage_t& o)
    {
        _thunk_size = o._thunk_size; 
        _every = o._every; 
        _first_chunk_index = o._first_chunk_index; 
    }

    SelectThunkEvery_parameter_storage_t( int thunk_size, int every, int first_chunk_index )
    {
        _thunk_size = thunk_size; 
        _every = every; 
        _first_chunk_index = first_chunk_index; 
    }

    SelectThunkEvery_parameter_storage_t( AVSValue args )
    {
        _thunk_size = SELECTTHUNKEVERY_ARG(thunk_size).AsInt();
        _every = SELECTTHUNKEVERY_ARG(every).AsInt();
        _first_chunk_index = SELECTTHUNKEVERY_ARG(first_chunk_index).AsInt(0);
    }
};

#define SELECTTHUNKEVERY_CREATE_CLASS(klass) new klass( child, SelectThunkEvery_parameter_storage_t( thunk_size, every, first_chunk_index ) )

#ifdef SELECTTHUNKEVERY_SIMPLE_MACRO_NAME

#ifdef SIMPLE_MACRO_NAME
#error Simple macro name has already been defined for SIMPLE_MACRO_NAME
#endif

#define SIMPLE_MACRO_NAME SelectThunkEvery

#define ARG SELECTTHUNKEVERY_ARG

#define CREATE_CLASS SELECTTHUNKEVERY_CREATE_CLASS

#endif

#ifdef SELECTTHUNKEVERY_IMPLEMENTATION

AVSValue __cdecl Create_SelectThunkEvery(AVSValue args, void* user_data, IScriptEnvironment* env);

void Register_SelectThunkEvery(IScriptEnvironment* env, void* user_data=NULL)
{
    env->AddFunction("SelectThunkEvery", 
        SELECTTHUNKEVERY_AVS_PARAMS,
        Create_SelectThunkEvery,
        user_data);
}

#else

void Register_SelectThunkEvery(IScriptEnvironment* env, void* user_data=NULL);

#endif

