#define MP_PIPELINE_IMPLEMENTATION

#include "stdafx.h"
#include "MP_Pipeline.h"
#include "slave_common.h"
#include "statement.h"
#include "utils.h"

#include <regex>
#include <functional>

#define BRANCH_STATEMENT_START "^\\s*### branch:"
#define BRANCH_STATEMENT_PARAM "\\s*(\\d+)(?:\\s*,\\s*(\\d{1,7}))?\\s*$"

#define PLATFORM_SCAN_FORMAT "%16s"
#define PLATFORM_PATTERN "^\\s*### platform:\\s*(\\w+)\\s*$"

#define PREFETCH_STATEMENT_START "^\\s*### prefetch:"
#define PREFETCH_STATEMENT_PARAM "\\s*(\\d+)\\s*,\\s*(\\d+)\\s*$"

#define EXPORT_CLIP_STATEMENT_START "^\\s*### export clip:"
#define PASS_CLIP_STATEMENT_START "^\\s*### pass clip:"
#define CLIP_LIST_STATEMENT_PARAM "\\s*((?:[_A-Za-z][_A-Za-z0-9]*\\s*(?:,\\s*|$))+)"

#define TAG_INHERIT_START "### inherit start ###"
#define TAG_INHERIT_END "### inherit end ###"

#define LOAD_PLUGIN_FUNCTION_NAME "MPP_Load"
#define GET_UPSTREAM_CLIP_FUNCTION_NAME "MPP_GetUpstreamClip"
#define PREPARE_DOWNSTREAM_CLIP_FUNCTION_NAME "MPP_PrepareDownstreamClip"

static const int DEFAULT_THUNK_SIZE = 1;

using namespace std;

AVSValue __cdecl Create_MP_Pipeline(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new MP_Pipeline(MP_Pipeline_parameter_storage_t(args), env);
}

MP_Pipeline::MP_Pipeline(const MP_Pipeline_parameter_storage_t& o, IScriptEnvironment* env)
    : MP_Pipeline_parameter_storage_t(o)
{
    memset(&_slave_stdin_handles, NULL, sizeof(_slave_stdin_handles));

    // the job object is used to make the system kill all slave processes if the main process is unexpectedly terminated
    _slave_job = CreateJobObject(NULL, NULL);
    if (!_slave_job)
    {
        env->ThrowError("MP_Pipeline: Unable to create job object, code = %d", GetLastError());
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info;
    memset(&limit_info, 0, sizeof(limit_info));
    limit_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    if (!SetInformationJobObject(_slave_job, JobObjectExtendedLimitInformation, &limit_info, sizeof(limit_info)))
    {
        env->ThrowError("MP_Pipeline: SetInformationJobObject failed, code = %d", GetLastError());
    }
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
    if (_slave_job)
    {
        // since we are exiting normally, reset the job limitation to default,
        // so that the slave processes can clean up and terminate themselves,
        // and the main process don't need to wait for them
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info;
        memset(&limit_info, 0, sizeof(limit_info));
        SetInformationJobObject(_slave_job, JobObjectExtendedLimitInformation, &limit_info, sizeof(limit_info));
        CloseHandle(_slave_job);
        _slave_job = NULL;
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

bool process_clip_list_statement(const char* statement_start, const char* statement_full, const char* script, function<void (const char*)> callback)
{
    char* statement_param;
    if (!has_statement(script, statement_start))
    {
        return true;
    }
    if (!scan_statement(script, statement_full, NULL, 
        NULL, &statement_param,
        NULL, NULL))
    {
        return false;
    }
    const char* current_pos = statement_param;
    regex re("^\\s*([_A-Za-z0-9]+)\\s*(?:,|$)", regex::ECMAScript);
    cmatch m;
    while (*current_pos)
    {
        if (!regex_search(current_pos, m, re))
        {
            free(statement_param);
            return false;
        }
        callback(m[1].str().c_str());
        current_pos = m[0].second;
    }
    free(statement_param);
    return true;
}

bool process_export_clip_statement(const char* script, function<void (const char*)> callback)
{
    return process_clip_list_statement(EXPORT_CLIP_STATEMENT_START, EXPORT_CLIP_STATEMENT_START CLIP_LIST_STATEMENT_PARAM, script, callback);
}

void append_import_clip_signature(char* s, const char* clip_var_name)
{
    sprintf_append(s, "### imported clip: %s ###", clip_var_name);
}

void process_pass_clip_statement(char* script, char* next_script, IScriptEnvironment* env)
{
    bool result = process_clip_list_statement(PASS_CLIP_STATEMENT_START, PASS_CLIP_STATEMENT_START CLIP_LIST_STATEMENT_PARAM, script,
        [=] (const char* clip_var_name) {
            char* pattern = (char*)malloc(strlen(clip_var_name) + 256);
            strcpy(pattern, "^.*");
            append_import_clip_signature(pattern, clip_var_name);
            strcat(pattern, "$");
            regex re(pattern, regex::ECMAScript);
            free(pattern);
            pattern = NULL;

            cmatch m;
            if (regex_search(script, m, re))
            {
                strcat(next_script, m[0].str().c_str());
                strcat(next_script, "\n");
            } else {
                env->ThrowError("MP_Pipeline: %s is not an imported clip.", clip_var_name);
            }
        }
    );    
    if (!result)
    {
        env->ThrowError("MP_Pipeline: Invalid pass clip statement.");
    }
}

void prepare_export_clip(char* script, char* next_script, int process_id, IScriptEnvironment* env)
{
    int i = 0;
    bool result = process_export_clip_statement(script, [=, &i] (const char* clip_var_name) {
        i++;
        sprintf_append(script, "%s = %s.%s()\n", clip_var_name, clip_var_name, PREPARE_DOWNSTREAM_CLIP_FUNCTION_NAME);
        sprintf_append(next_script, "%s = %s_%d(%d) ", clip_var_name, GET_UPSTREAM_CLIP_FUNCTION_NAME, process_id, i);
        append_import_clip_signature(next_script, clip_var_name);
        strcat(next_script, "\n");
    });
    if (!result)
    {
        env->ThrowError("MP_Pipeline: Invalid export statement");
    }
}

void MP_Pipeline::prepare_slave(slave_create_params* params, IScriptEnvironment* env)
{
    params->slave_job_object = _slave_job;
    scan_statement(params->script, PLATFORM_PATTERN, NULL, PLATFORM_SCAN_FORMAT, params->slave_platform);

    sprintf_append(params->script, "MPP_SharedMemoryServer(%s()", GET_OUTPUT_PORT_FUNCTION_NAME);

    auto export_callback = [params] (const char* clip_var_name) {
        sprintf_append(params->script, ", %s", clip_var_name);
    };

    if (!process_export_clip_statement(params->script, export_callback))
    {
        env->ThrowError("MP_Pipeline: Unexpected error: process_export_clip_statement in prepare_slave failed.");
    }

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

void copy_inherit_block(const char* source, char* target)
{
    char pattern[256];
    sprintf(pattern, "^\\s*%s\\s*$(?:.|\\s)*?^\\s*%s\\s*$", TAG_INHERIT_START, TAG_INHERIT_END);

    regex re(pattern, regex::ECMAScript);
    cmatch m;

    while (regex_search(source, m, re))
    {
        strncat(target, m[0].first, m[0].length());
        strcat(target, "\n");
        source = m[0].second;
    }
}

void begin_get_upstream_clip_function(char* script, int process_id)
{
    sprintf_append(script, "%s\nfunction %s_%d(int clip_index) {\n", TAG_INHERIT_START, GET_UPSTREAM_CLIP_FUNCTION_NAME, process_id);
}

void end_get_upstream_clip_function(char* script)
{
    sprintf_append(script, "}\n%s\n", TAG_INHERIT_END);
}

void begin_prepare_downstream_clip_function(char* script)
{
    sprintf_append(script, "function %s(clip c) {\n", PREPARE_DOWNSTREAM_CLIP_FUNCTION_NAME);
}

void end_prepare_downstream_clip_function(char* script)
{
    strcat(script, "}\n");
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
    int process_id = *slave_count;
    char* buffer = (char*)malloc(buffer_size);
    __try
    {
        memset(buffer, 0, buffer_size);

        const char* GET_BRANCH_ID = "MPP_GetBranchID";
        _snprintf(buffer, buffer_size, "global MP_PIPELINE_BRANCH_ID = %s()\n%s\n%s()\n", GET_BRANCH_ID, script, PREPARE_DOWNSTREAM_CLIP_FUNCTION_NAME);
        prepare_export_clip(buffer, next_script, process_id, env);
        sprintf_append(buffer, "%s\n", branch_line_ptr + 1);

        char* script_end = buffer + strlen(buffer);

        begin_get_upstream_clip_function(next_script, process_id);
        sprintf_append(next_script, "return ThunkedInterleave(%d,", thunk_size);

        for (int i = 0; i < branch_count; i++)
        {
            if ((*slave_count) >= MAX_SLAVES)
            {
                env->ThrowError("MP_Pipeline: Too many slaves.");
            }
            *script_end = NULL;

            sprintf_append(buffer, "function %s() {\n return %d \n}\n", GET_BRANCH_ID, i);

            begin_prepare_downstream_clip_function(buffer);
            sprintf_append(buffer, "return c.SelectThunkEvery(%d, %d, %d)\n", thunk_size, branch_count, i);
            end_prepare_downstream_clip_function(buffer);
            
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
        end_get_upstream_clip_function(next_script);
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
        AVSValue value = env->Invoke("Eval", script);
        if (!value.IsClip())
        {
            throw AvisynthError("Not a clip.");
        }
        SetChild(value.AsClip());
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
        size_t buffer_size = strlen(_script) + 256000;
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

            int process_id = slave_count;
            sprintf_append(next_script_part, "%s_%d(0)\n", GET_UPSTREAM_CLIP_FUNCTION_NAME, process_id);

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

            strcat(current_script_part, current_pos);
            copy_inherit_block(current_script_part, next_script_part);

            process_pass_clip_statement(current_script_part, next_script_part, env);

            if (has_statement(current_script_part, BRANCH_STATEMENT_START))
            {
                create_branch(current_script_part, next_script_part, &slave_count, env);
                port = 0;
            } else {
                sprintf_append(current_script_part, "%s()\n", PREPARE_DOWNSTREAM_CLIP_FUNCTION_NAME);
                prepare_export_clip(current_script_part, next_script_part, process_id, env);

                begin_prepare_downstream_clip_function(current_script_part);
                strcat(current_script_part, "return c\n");
                end_prepare_downstream_clip_function(current_script_part);

                slave_create_params params;
                memset(&params, 0, sizeof(params));
                params.filter_name = "MP_Pipeline";
                params.script = current_script_part;
                prepare_slave(&params, env);

                create_slave(env, &params, &port, _slave_stdin_handles + slave_count);

                begin_get_upstream_clip_function(next_script_part, process_id);
                sprintf_append(next_script_part, "return " TCPSOURCE_TEMPLATE "\n", port);
                end_get_upstream_clip_function(next_script_part);
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