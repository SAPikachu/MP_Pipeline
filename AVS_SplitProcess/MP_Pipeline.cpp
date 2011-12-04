#define MP_PIPELINE_IMPLEMENTATION

#include "stdafx.h"
#include "MP_Pipeline.h"
#include "slave_common.h"

AVSValue __cdecl Create_MP_Pipeline(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new MP_Pipeline(MP_Pipeline_parameter_storage_t(args), env);
}

MP_Pipeline::MP_Pipeline(const MP_Pipeline_parameter_storage_t& o, IScriptEnvironment* env)
    : MP_Pipeline_parameter_storage_t(o)
{
    memset(&_slave_stdin_handles, NULL, sizeof(_slave_stdin_handles));
    this->create_pipeline(env);
}

MP_Pipeline::~MP_Pipeline()
{
    for (int i = 0; i < MAX_SLAVES; i++)
    {
        if (!_slave_stdin_handles[i])
        {
            break;
        }
        CloseHandle(_slave_stdin_handles[i]);
        _slave_stdin_handles[i] = NULL;
    }
}

void build_branch_sink(char* buffer, const char* script, int* branch_ports)
{
    bool first_part = true;
    char part_buffer[64];
    strcpy(buffer, "Interleave(");
    while (*branch_ports)
    {
        memset(part_buffer, 0, sizeof(part_buffer));
        _snprintf(part_buffer, sizeof(part_buffer) - 1, "%s" TCPSOURCE_TEMPLATE, first_part ? "" : ",", *branch_ports);
        strcat(buffer, part_buffer);
        first_part = false;
        branch_ports++;
    }
    strcat(buffer, ")\n");
    strcat(buffer, script);
}

void MP_Pipeline::create_branch(char* script, char* branch_line_ptr, int* branch_ports, int source_port, int* slave_count, IScriptEnvironment* env)
{
    int branch_count = 0;
    sscanf(branch_line_ptr, BRANCH_LINE_START "%d", &branch_count);
    if (branch_count < 2 || branch_count >= MAX_SLAVES)
    {
        env->ThrowError("MP_Pipeline: Invalid branch count: %d", branch_count);
    }
    *branch_line_ptr = NULL;
    size_t buffer_size = strlen(script) + 1024;
    char* buffer = (char*)malloc(buffer_size);
    __try
    {
        for (int i = 0; i < branch_count; i++)
        {
            memset(buffer, 0, buffer_size);
            _snprintf(buffer, buffer_size, "%s\nSelectEvery(%d, %d)\n%s", script, branch_count, i, branch_line_ptr + 1);
            create_slave(env, "MP_Pipeline", buffer, source_port, branch_ports + i, _slave_stdin_handles + *slave_count);
            (*slave_count)++;
        }
    }
    __finally
    {
        free(buffer);
    }
}

// workaround for C2712
void MP_Pipeline::create_pipeline_finish(char* script, IScriptEnvironment* env)
{
    try
    {
        SetChild(env->Invoke("Eval", script).AsClip());
    }
    catch (AvisynthError& e)
    {
        env->ThrowError("MP_Pipeline: Error while creating last part of the filter chain:\n%s", e.msg);
    }
}

char* build_part_script(char *buffer, size_t buffer_size, int* branch_ports, char *script)
{
    char* current_script;
    if (branch_ports[0] == 0)
    {
        current_script = script;
    } else {
        memset(buffer, 0, buffer_size);
        build_branch_sink(buffer, script, branch_ports);
        current_script = buffer;
        memset(branch_ports, 0, sizeof(branch_ports));
    }
    return current_script;
}

void MP_Pipeline::create_pipeline(IScriptEnvironment* env)
{
    char* script_dup = _strdup(_script);
    char* buffer = NULL;
    int branch_ports[MAX_SLAVES + 1];
    memset(branch_ports, NULL, sizeof(branch_ports));
    __try
    {
        size_t buffer_size = strlen(_script) + 25600;
        buffer = (char*)malloc(buffer_size);
        char* current_pos = script_dup;
        int slave_count = 0;
        int port = 0;
        while (true)
        {
            char* splitter_pos = strstr(current_pos, SCRIPT_SPLITTER);
            if (!splitter_pos)
            {
                break;
            }
            if (slave_count >= MAX_SLAVES)
            {
                env->ThrowError("MP_Pipeline: Too many splitters");
            }
            *splitter_pos = 0;
            char* current_script = NULL;
            current_script = build_part_script(buffer, buffer_size, branch_ports, current_pos);
            char* branch_line_ptr = strstr(current_script, BRANCH_LINE_START);
            if (branch_line_ptr)
            {
                create_branch(current_script, branch_line_ptr, branch_ports, port, &slave_count, env);
                port = 0;
            } else {
                create_slave(env, "MP_Pipeline", current_script, port, &port, _slave_stdin_handles + slave_count);
            }
            current_pos = splitter_pos + strlen(SCRIPT_SPLITTER);
            slave_count++;
        }
        if (slave_count == 0)
        {
            env->ThrowError("MP_Pipeline: Can't find any splitter.");
        }
        
        if (branch_ports[0] == 0)
        {
            _snprintf(buffer, buffer_size, TCPSOURCE_TEMPLATE "\n", port);
            strcat(buffer, current_pos);
        } else {
            build_part_script(buffer, buffer_size, branch_ports, current_pos);
        }
        create_pipeline_finish(buffer, env);
    }
    __finally
    {
        free(buffer);
        free(script_dup);
    }
}