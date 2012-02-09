#define MP_PIPELINE_IMPLEMENTATION

#include "stdafx.h"
#include "MP_Pipeline.h"
#include "slave_common.h"
#include "statement.h"
#include "utils.h"

#define BRANCH_STATEMENT_START "^\\s*### branch:"
#define BRANCH_STATEMENT_PARAM "\\s*(\\d+)(?:\\s*,\\s*(\\d{1,7}))?\\s*$"

#define PLATFORM_SCAN_FORMAT "%16s"
#define PLATFORM_PATTERN "^\\s*### platform:\\s*(\\w+)\\s*$"

#define PREFETCH_STATEMENT_START "^\\s*### prefetch:"
#define PREFETCH_STATEMENT_PARAM "\\s*(\\d+)\\s*,\\s*(\\d+)\\s*$"

#define EXPORT_CLIP_STATEMENT_START "^\\s*### export clip:"
#define EXPORT_CLIP_STATEMENT_PARAM "\\s*((?:[_A-Za-z][_A-Za-z0-9]*\\s*(?:,\\s*|$))+)"

#define LOAD_PLUGIN_FUNCTION_NAME "Load_MP_Pipeline"

static const int DEFAULT_THUNK_SIZE = 1;

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

void add_load_plugin_function(char* script, const char* platform, IScriptEnvironment* env)
{
    char self_path[32768];
    if (!get_self_path(self_path, ARRAYSIZE(self_path) - 32))
    {
        env->ThrowError("get_self_path failed, code = %d", GetLastError());
    }
    append_platform_if_needed(self_path, platform);
    sprintf_append(script, "function %s() {\n LoadPlugin(\"%s\")\n }\n", LOAD_PLUGIN_FUNCTION_NAME, self_path);
}

void prepare_slave(slave_create_params* params, IScriptEnvironment* env)
{
    scan_statement(params->script, PLATFORM_PATTERN, NULL, PLATFORM_SCAN_FORMAT, params->slave_platform);

    sprintf_append(params->script, "MPP_TCPServer(%s()", GET_OUTPUT_PORT_FUNCTION_NAME);
    if (has_statement(params->script, PREFETCH_STATEMENT_START))
    {
        int max_cache_frames, cache_behind;
        if (!scan_statement(params->script, PREFETCH_STATEMENT_START PREFETCH_STATEMENT_PARAM, NULL, 
            "%d", &max_cache_frames,
            "%d", &cache_behind,
            NULL, NULL))
        {
            env->ThrowError("MP_Pipeline: Invalid cache statement.");
        }
        sprintf_append(params->script, ", max_cache_frames=%d, cache_behind=%d", max_cache_frames, cache_behind);
    }
    strcat(params->script, ")\n");

    add_load_plugin_function(params->script, params->slave_platform, env);
}

void MP_Pipeline::create_branch(char* script, char* next_script, int* slave_count, IScriptEnvironment* env)
{
    int branch_count = 0;
    char* thunk_size_str = NULL;
    char* branch_line_ptr = NULL;
    int thunk_size = DEFAULT_THUNK_SIZE;

    if (!scan_statement(script, BRANCH_STATEMENT_START BRANCH_STATEMENT_PARAM, &branch_line_ptr, "%d", &branch_count, NULL, &thunk_size_str, NULL, NULL))
    {
        env->ThrowError("MP_Pipeline: Invalid branch statement.");
    }
    if (thunk_size_str && strlen(thunk_size_str) > 0)
    {
        sscanf(thunk_size_str, "%d", &thunk_size);
    }
    // free(thunk_size_str);
    if (branch_count < 2 || branch_count >= MAX_SLAVES)
    {
        env->ThrowError("MP_Pipeline: Invalid branch count: %d", branch_count);
    }
    if (thunk_size <= 0)
    {
        env->ThrowError("MP_Pipeline: Invalid thunk size: %d", thunk_size);
    }
    *branch_line_ptr = NULL;
    size_t buffer_size = strlen(script) + 1024;
    char* buffer = (char*)malloc(buffer_size);
    __try
    {
        sprintf_append(next_script, "ThunkedInterleave(%d,", thunk_size);
        for (int i = 0; i < branch_count; i++)
        {
            if ((*slave_count) >= MAX_SLAVES)
            {
                env->ThrowError("MP_Pipeline: Too many slaves.");
            }
            memset(buffer, 0, buffer_size);
            _snprintf(buffer, buffer_size, "global MP_PIPELINE_BRANCH_ID = %d\n%s\nSelectThunkEvery(%d, %d, %d)\n%s", i, script, thunk_size, branch_count, i, branch_line_ptr + 1);

            
            slave_create_params params;

            memset(&params, 0, sizeof(params));
            params.filter_name = "MP_Pipeline";
            params.script = buffer;
            prepare_slave(&params, env);

            int port = -1;
            create_slave(env, &params, &port, _slave_stdin_handles + *slave_count);

            sprintf_append(next_script, "%s" TCPSOURCE_TEMPLATE, i == 0 ? "" : ", ", port);

            (*slave_count)++;
        }
        strcat(next_script, ")\n");
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

void MP_Pipeline::create_pipeline(IScriptEnvironment* env)
{
    char* script_dup = _strdup(_script);
    char* buffer_store = NULL;
    char* current_script_part = NULL;
    char* next_script_part = NULL;
    int branch_ports[MAX_SLAVES + 1];
    memset(branch_ports, NULL, sizeof(branch_ports));
    __try
    {
        size_t buffer_size = strlen(_script) + 25600;
        buffer_store = (char*)malloc(buffer_size * 2);
        memset(buffer_store, 0, buffer_size * 2);
        current_script_part = buffer_store;
        next_script_part = buffer_store + buffer_size;
        char* current_pos = script_dup;
        int slave_count = 0;
        int port = 0;

        sprintf(current_script_part, "%s()\n", LOAD_PLUGIN_FUNCTION_NAME);
        while (true)
        {
            sprintf(next_script_part, "%s()\n", LOAD_PLUGIN_FUNCTION_NAME);
            char* splitter_pos = NULL;
            char* splitter_line = NULL;
            if (!scan_statement(current_pos, SCRIPT_SPLITTER_PATTERN, &splitter_pos, NULL, &splitter_line))
            {
                break;
            }
            // there may be spaces in the splitter line, so we must get its length in runtime
            size_t splitter_length = strlen(splitter_line);
            free(splitter_line);
            splitter_line = NULL;
            if (slave_count >= MAX_SLAVES)
            {
                env->ThrowError("MP_Pipeline: Too many slaves");
            }
            // terminate current script part
            memset(splitter_pos, 0, splitter_length);

            // must use strcat here, since buffer may already have content (filled by previous part)
            strcat(current_script_part, current_pos);

            if (has_statement(current_script_part, BRANCH_STATEMENT_START))
            {
                create_branch(current_script_part, next_script_part, &slave_count, env);
                port = 0;
            } else {
                slave_create_params params;
                memset(&params, 0, sizeof(params));
                params.filter_name = "MP_Pipeline";
                params.script = current_script_part;
                prepare_slave(&params, env);

                create_slave(env, &params, &port, _slave_stdin_handles + slave_count);
                sprintf_append(next_script_part, TCPSOURCE_TEMPLATE "\n", port);
            }
            current_pos = splitter_pos + splitter_length;

            char* swap_tmp = current_script_part;
            current_script_part = next_script_part;
            next_script_part = swap_tmp;

            slave_count++;
        }
        if (slave_count == 0)
        {
            env->ThrowError("MP_Pipeline: Can't find any splitter.");
        }
        strcat(current_script_part, current_pos);
        sprintf_append(current_script_part, "function %s() {}\n", LOAD_PLUGIN_FUNCTION_NAME);
        create_pipeline_finish(current_script_part, env);
    }
    __finally
    {
        free(buffer_store);
        free(script_dup);
    }
}