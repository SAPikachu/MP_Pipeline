#define MP_PIPELINE_IMPLEMENTATION

#include "stdafx.h"
#include "MP_Pipeline.h"
#include "slave_common.h"
#include "statement.h"

#define BRANCH_STATEMENT_START "^\\s*### branch: "

#define BRANCH_STATEMENT_PARAM "(\\d+)\\s*$"


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

void MP_Pipeline::create_branch(char* script, int* branch_ports, int source_port, int* slave_count, IScriptEnvironment* env)
{
    int branch_count = 0;
    char* branch_line_ptr = NULL;
    if (!scan_statement(script, BRANCH_STATEMENT_START BRANCH_STATEMENT_PARAM, &branch_line_ptr, "%d", &branch_count))
    {
        env->ThrowError("MP_Pipeline: Invalid branch statement.");
    }
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
            if ((*slave_count) >= MAX_SLAVES)
            {
                env->ThrowError("MP_Pipeline: Too many slaves.");
            }
            memset(buffer, 0, buffer_size);
            _snprintf(buffer, buffer_size, "%s\nSelectEvery(%d, %d)\n%s", script, branch_count, i, branch_line_ptr + 1);

            
            slave_create_params params;
            memset(&params, 0, sizeof(params));
            params.filter_name = "MP_Pipeline";
            params.source_port = source_port;
            params.script = buffer;

            create_slave(env, &params, branch_ports + i, _slave_stdin_handles + *slave_count);
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
            char* splitter_pos = NULL;
            char* splitter_line = NULL;
            if (!scan_statement(current_pos, SCRIPT_SPLITTER_PATTERN, &splitter_pos, NULL, &splitter_line))
            {
                break;
            }
            // there may be spaces in the splitter line, so we must get its length in runtime
            int splitter_length = strlen(splitter_line);
            free(splitter_line);
            splitter_line = NULL;
            if (slave_count >= MAX_SLAVES)
            {
                env->ThrowError("MP_Pipeline: Too many slaves");
            }
            // terminate current script part
            memset(splitter_pos, 0, splitter_length);
            char* current_script = NULL;
            current_script = build_part_script(buffer, buffer_size, branch_ports, current_pos);
            if (has_statement(current_script, BRANCH_STATEMENT_START))
            {
                create_branch(current_script, branch_ports, port, &slave_count, env);
                port = 0;
            } else {
                slave_create_params params;
                memset(&params, 0, sizeof(params));
                params.filter_name = "MP_Pipeline";
                params.source_port = port;
                params.script = current_script;

                create_slave(env, &params, &port, _slave_stdin_handles + slave_count);
            }
            current_pos = splitter_pos + splitter_length;
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