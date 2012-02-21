#include "stdafx.h"
#include "SharedMemoryClient.h"

using namespace std;

SharedMemoryClient::SharedMemoryClient(SharedMemoryClient_parameter_storage_t& o, IScriptEnvironment* env) :
    SharedMemoryClient_parameter_storage_t(o),
    _manager(get_shared_memory_key("LOCAL", _port))
{

}