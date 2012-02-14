#pragma once

#include "SharedMemoryClient.def.h"
#include "avisynth.h"

class SharedMemoryClient : public IClip, private SharedMemoryClient_parameter_storage_t
{
    SharedMemoryClient(SharedMemoryClient_parameter_storage_t& o, IScriptEnvironment* env);
};