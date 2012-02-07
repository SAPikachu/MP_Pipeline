// AVS_SplitProcess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string.h>
#include "avisynth.h"
#include "splitprocess.h"
#include <memory>


int _tmain(int argc, _TCHAR* argv[])
{
    TCHAR script[32768];
    memset(script, 0, sizeof(script));
    if (!GetEnvironmentVariable(SCRIPT_VAR_NAME, script, ARRAYSIZE(script) - 1))
    {
        printf("GetEnvironmentVariable failed, code = %d.\n", GetLastError());
        return 1;
    }
    if (strlen(script) == 0)
    {
        printf("Script is empty.\n");
        return 2;
    }
    HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (!stdin_handle)
    {
        printf("Standard input not redirected.\n");
        return 5;
    } else if (stdin_handle == INVALID_HANDLE_VALUE) {
        printf("GetStdHandle failed, code = %d.\n", GetLastError());
        return 6;
    }
    IScriptEnvironment* env = CreateScriptEnvironment();
    {
        AVSValue v;
        AVSValue script_value = script;
        try
        {
            v = env->Invoke("Eval", script_value);
        }
        catch (IScriptEnvironment::NotFound)
        {
            printf("Unexpected exception: Filter not found.\n");
            return 3;
        }
        catch (AvisynthError e)
        {
            printf("Script error: %s\n", e.msg);
            return 4;
        }
        catch (...)
        {
            printf("Unknown exception\n");
            return 7;
        }
        printf(SLAVE_OK_FLAG "\n");
        fflush(stdout);
        char dummy_buffer[256];
        DWORD bytes_read = 0;
        do
        {
            if (!ReadFile(stdin_handle, dummy_buffer, sizeof(dummy_buffer), &bytes_read, NULL))
            {
                printf("ReadFile failed, code = %d.\n", GetLastError());
                break;
            }
        } while (bytes_read > 0);
        printf("Terminating...\n");
    }
    return 0;
}

