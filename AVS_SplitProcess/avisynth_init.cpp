#include "stdafx.h"
#include "MP_Pipeline.h"
#include "SelectThunkEvery.h"

__declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
    Register_MP_Pipeline(env);
    Register_SelectThunkEvery(env);

    return "MP";
}