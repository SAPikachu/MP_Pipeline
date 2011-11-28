#include <Windows.h>
#include "avisynth.h"

#define TCPSOURCE_TEMPLATE "TCPSource(\"127.0.0.1\", %d, \"None\")"

void create_slave(IScriptEnvironment* env,  const char* filter_name,const char* script, int source_port, int* new_slave_port, HANDLE* slave_stdin_handle);