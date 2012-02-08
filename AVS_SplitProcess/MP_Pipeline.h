#pragma once

#include "MP_Pipeline.def.h"
#include "GenericVideoFilter2.h"

static const char* SCRIPT_SPLITTER_PATTERN = "^(\\s*### ###\\s*)$";

static const int MAX_SLAVES = 255;


class MP_Pipeline : public GenericVideoFilter2, MP_Pipeline_parameter_storage_t
{
public:
    MP_Pipeline(const MP_Pipeline_parameter_storage_t& o, IScriptEnvironment* env);
    virtual ~MP_Pipeline();
private:
    void create_pipeline(IScriptEnvironment* env);

    void create_branch(char* script, char* next_script, int* slave_count, int* thunk_size, IScriptEnvironment* env);
    void create_pipeline_finish(char* script, IScriptEnvironment* env);

    HANDLE _slave_stdin_handles[MAX_SLAVES + 1];
};
