#include "stdafx.h"
#include "MP_Pipeline.h"

__declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
    Register_MP_Pipeline(env);

    return "MP";
}