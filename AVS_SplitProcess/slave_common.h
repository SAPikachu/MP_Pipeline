#include <Windows.h>
#include "avisynth.h"

#define TCPSOURCE_TEMPLATE "TCPSource(\"127.0.0.1\", %d, \"None\")"

typedef struct _slave_create_params
{
    const char* filter_name;
    const char* script;
    int source_port;
} slave_create_params;

void create_slave(IScriptEnvironment* env, slave_create_params* params, int* new_slave_port, HANDLE* slave_stdin_handle);