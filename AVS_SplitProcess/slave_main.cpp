// AVS_SplitProcess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string.h>
#include "avisynth.h"
#include "splitprocess.h"
#include <memory>
#include <assert.h>
#include <io.h>
#include <stdlib.h>
#include <fcntl.h>

typedef IScriptEnvironment* (__stdcall *CreateScriptEnvironment_t)(int);

int _tmain(int argc, _TCHAR* argv[])
{
    HANDLE stdout_handle_original = GetStdHandle(STD_OUTPUT_HANDLE);
    assert(stdout_handle_original != INVALID_HANDLE_VALUE);
    HANDLE stdout_handle = INVALID_HANDLE_VALUE;
    DuplicateHandle(GetCurrentProcess(), stdout_handle_original, GetCurrentProcess(), &stdout_handle, 0, FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS);
    assert(stdout_handle != INVALID_HANDLE_VALUE);
    SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE));

    auto stdout_file = _fdopen(_open_osfhandle((intptr_t)stdout_handle, _O_TEXT), "w");
    TCHAR script[32768];

    memset(script, 0, sizeof(script));
    if (!GetEnvironmentVariable(SCRIPT_VAR_NAME, script, ARRAYSIZE(script) - 1))
    {
        fprintf(stdout_file, "GetEnvironmentVariable failed, code = %d.\n", GetLastError());
        return 1;
    }
    if (strlen(script) == 0)
    {
        fprintf(stdout_file, "Script is empty.\n");
        return 2;
    }
    HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (!stdin_handle)
    {
        fprintf(stdout_file, "Standard input not redirected.\n");
        return 5;
    } else if (stdin_handle == INVALID_HANDLE_VALUE) {
        fprintf(stdout_file, "GetStdHandle failed, code = %d.\n", GetLastError());
        return 6;
    }
    const TCHAR* avisynth_module_name = TEXT("avisynth.dll");
    HMODULE avisynth_module = LoadLibrary(avisynth_module_name);
    if (!avisynth_module)
    {
        fprintf(stdout_file, "Unable to load %s, code = %d\n", avisynth_module_name, GetLastError());
        return 9;
    }
    CreateScriptEnvironment_t create_script_env = (CreateScriptEnvironment_t)GetProcAddress(avisynth_module, "CreateScriptEnvironment");
    if (!create_script_env)
    {
        fprintf(stdout_file, "%s doesn't contain entry point of CreateScriptEnvironment, code = %d\n", avisynth_module_name, GetLastError());
        return 10;
    }

    IScriptEnvironment* env = create_script_env(AVISYNTH_INTERFACE_VERSION);
    {
        AVSValue v;
        AVSValue script_value = script;
        try
        {
            v = env->Invoke("Eval", script_value);
        }
        catch (IScriptEnvironment::NotFound)
        {
            fprintf(stdout_file, "Unexpected exception: Filter not found.\n");
            fflush(stdout_file);
            return 3;
        }
        catch (AvisynthError e)
        {
            fprintf(stdout_file, "Script error: %s\n", e.msg);
            fflush(stdout_file);
            return 4;
        }
        catch (...)
        {
            fprintf(stdout_file, "Unknown exception\n");
            fflush(stdout_file);
            return 7;
        }
        fprintf(stdout_file, SLAVE_OK_FLAG "\n");
        fflush(stdout_file);
        char dummy_buffer[256];
        DWORD bytes_read = 0;
        do
        {
            if (!ReadFile(stdin_handle, dummy_buffer, sizeof(dummy_buffer), &bytes_read, NULL))
            {
                fprintf(stdout_file, "ReadFile failed, code = %d.\n", GetLastError());
                break;
            }
        } while (bytes_read > 0);
        fprintf(stdout_file, "Terminating...\n");
        fflush(stdout_file);
    }
    // It is not safe to delete env here since env is not allocated by us,
    // and we don't have env->DeleteScriptEnvironment on pre-2.6,
    // so we just call its destructor to let plugins clean up themselves
    env->~IScriptEnvironment();

    // To prevent problems we don't free it, since we are exiting anyways
    // FreeLibrary(avisynth_module);
    return 0;
}

