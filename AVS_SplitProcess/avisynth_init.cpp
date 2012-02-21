#include "stdafx.h"
#include "MP_Pipeline.h"
#include "SelectThunkEvery.h"
#include "ThunkedInterleave.h"
#include "TCPDeliver/TCPDeliver.h"
#include "SharedMemoryClient.h"
#include "SharedMemoryServer.h"

__declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
    Register_MP_Pipeline(env);
    Register_SelectThunkEvery(env);
    Register_ThunkedInterleave(env);
    Register_TCPDeliver(env);
    Register_SharedMemoryClient(env);
    Register_SharedMemoryServer(env);

    return "MP";
}