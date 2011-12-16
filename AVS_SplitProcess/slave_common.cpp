#define UNICODE
#define _UNICODE

#include "stdafx.h"

#include <Windows.h>
#include <stdio.h>
#include <WinSock2.h>

#include "slave_common.h"

#include "splitprocess.h"

#define TRACE_COMPONENT L"slave_common"
#include "trace.h"

#pragma comment(lib, "ws2_32.lib")

#ifdef UNICODE

#define PRINTF_TSTR "%S"
#define PRINTF_TSTR_T L"%S"
#define PRINTF_TSTR_L L"%S"

#else

#define PRINTF_TSTR "%s"
#define PRINTF_TSTR_T "%s"
#define PRINTF_TSTR_L L"%s"

#endif

static volatile BOOL _WSAStartup_called = FALSE;

static volatile HANDLE _startup_mutex = NULL;

int get_unused_port()
{
    if (!_WSAStartup_called)
    {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            TRACE("WSAStartup failed");
            return -1;
        }
        _WSAStartup_called = TRUE;
    }
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        TRACE("socket failed, code = %d", WSAGetLastError());
        return -2;
    }
    sockaddr_in addr;
    int name_len = sizeof(addr);
    memset(&addr, 0, name_len);
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
    addr.sin_port = 0;
    if (bind(sock, (sockaddr*)&addr, name_len) != 0)
    {
        TRACE("bind failed, code = %d", WSAGetLastError());
        return -3;
    }
    if (getsockname(sock, (sockaddr*)&addr, &name_len) != 0)
    {
        TRACE("getsockname failed, code = %d", WSAGetLastError());
        return -4;
    }
    closesocket(sock);
    return addr.sin_port;
}

BOOL get_self_path(TCHAR* path_out, int buffer_size)
{
    HMODULE module = NULL;
    if (!GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)get_self_path,
        &module))
    {
        TRACE("GetModuleHandleEx failed, code = %d", GetLastError());
        return FALSE;
    }
    return GetModuleFileName(module, path_out, buffer_size) != 0;
}

bool append_platform_if_needed(const char* requested_platform, TCHAR* buffer, size_t buffer_size) 
{
    if (strlen(requested_platform) > 0 && _stricmp(requested_platform, CURRENT_PLATFORM) != 0)
    {
        _tcsncat(buffer, TEXT("."), buffer_size - 1);

        TCHAR platform_buffer[MAX_PLATFORM_LENGTH + 1];
        memset(platform_buffer, 0, sizeof(platform_buffer));

#ifndef UNICODE
        strcpy(platform_buffer, params->slave_platform);
#else
        if (!MultiByteToWideChar(CP_ACP, 0, requested_platform, -1, platform_buffer, ARRAYSIZE(platform_buffer)))
        {
            return false;
        }
#endif
        _tcsncat(buffer, platform_buffer, buffer_size - 1);
    }
    return true;
}

#define TRACE_ERROR(format, ...) \
    do { \
        TRACE(format, __VA_ARGS__); \
        _snprintf(error_msg, error_msg_len, format, __VA_ARGS__); \
    } while (0)

#define CHECKED(name, ...) \
    do { \
        if (!name(__VA_ARGS__)) { \
            TRACE_ERROR("%s failed, code = %d", __TEXT(#name), GetLastError()); \
            return NULL; \
        } \
    } while (0)

HANDLE create_slave_process(slave_create_params* params, char* error_msg, size_t error_msg_len)
{
    TCHAR slave_exec[32768];
    memset(slave_exec, NULL, sizeof(slave_exec));

    if (!get_self_path(slave_exec, ARRAYSIZE(slave_exec) - 128))
    {
        TRACE_ERROR("Unable to get path of slave executable, code = %d", GetLastError());
        return NULL;
    }
    CHECKED(append_platform_if_needed, params->slave_platform, slave_exec, ARRAYSIZE(slave_exec) - 128);
    _tcsncat(slave_exec, TEXT(".slave.exe"), ARRAYSIZE(slave_exec) - 1);
    if (GetFileAttributes(slave_exec) == INVALID_FILE_ATTRIBUTES)
    {
        TRACE_ERROR("Can't find slave executable.");
        return NULL;
    }

    if (_startup_mutex == NULL)
    {
        HANDLE new_mutex = CreateMutex(NULL, FALSE, NULL);
        if (!new_mutex)
        {
            TRACE_ERROR("CreateMutex failed, code = %d", GetLastError());
            return NULL;
        }
        if (InterlockedCompareExchangePointer(&_startup_mutex, new_mutex, NULL) != NULL)
        {
            // other thread wins
            CloseHandle(new_mutex);
        }
    }
    bool success = false;
    HANDLE pipe_stdin_read = NULL, pipe_stdin_write = NULL;
    HANDLE pipe_stdout_read = NULL, pipe_stdout_write = NULL;

    WaitForSingleObject(_startup_mutex, INFINITE);
    __try
    {
        CHECKED(SetEnvironmentVariableA, SCRIPT_VAR_NAME_A, params->script);
        SECURITY_ATTRIBUTES pipe_attrs;
        memset(&pipe_attrs, 0, sizeof(pipe_attrs));
        pipe_attrs.nLength = sizeof(pipe_attrs);
        pipe_attrs.bInheritHandle = TRUE;

        CHECKED(CreatePipe, &pipe_stdin_read, &pipe_stdin_write, &pipe_attrs, 0);
        CHECKED(SetHandleInformation, pipe_stdin_write, HANDLE_FLAG_INHERIT, 0);

        CHECKED(CreatePipe, &pipe_stdout_read, &pipe_stdout_write, &pipe_attrs, 0);
        CHECKED(SetHandleInformation, pipe_stdout_read, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        memset(&si, NULL, sizeof(si));
        memset(&pi, NULL, sizeof(pi));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdInput = pipe_stdin_read;
        si.hStdError = pipe_stdout_write;
        si.hStdOutput = pipe_stdout_write;
        si.wShowWindow = SW_HIDE;
        CHECKED(CreateProcess, slave_exec, NULL, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(pipe_stdin_read);
        pipe_stdin_read = NULL;
        CloseHandle(pipe_stdout_write);
        pipe_stdout_write = NULL;

        memset(error_msg, 0, error_msg_len);
        DWORD bytes_read = 0;
        CHECKED(ReadFile, pipe_stdout_read, error_msg, (DWORD)error_msg_len - 1, &bytes_read, NULL);
        if (memcmp(error_msg, SLAVE_OK_FLAG, strlen(SLAVE_OK_FLAG)) == 0)
        {
            success = true;
        } else {
            DWORD cur_bytes_read = 0;
            do 
            {
                if (ReadFile(pipe_stdout_read, error_msg + bytes_read, (DWORD)(error_msg_len - 1 - bytes_read), &cur_bytes_read, NULL))
                {
                    bytes_read += cur_bytes_read;
                } else {
                    break;
                }
            } while (cur_bytes_read > 0 && bytes_read < error_msg_len -1);
        }
        CloseHandle(pipe_stdout_read);
        pipe_stdout_read = NULL;
        if (success)
        {
            return pipe_stdin_write;
        } else {
            return NULL;
        }
    }
    __finally
    {
        ReleaseMutex(_startup_mutex);
        if (!success)
        {
            CloseHandle(pipe_stdin_read);
            CloseHandle(pipe_stdin_write);
            CloseHandle(pipe_stdout_read);
            CloseHandle(pipe_stdout_write);
        }
    }
}

#undef TRACE_ERROR
#undef CHECKED

void create_slave(IScriptEnvironment* env, slave_create_params* params, int* new_slave_port, HANDLE* slave_stdin_handle)
{
    *new_slave_port = 0;
    *slave_stdin_handle = NULL;

    size_t new_script_size = strlen(params->script) + 4096;
    char* new_script = (char*)malloc(new_script_size);
    char buffer[1024];
    __try
    {
        memset(new_script, 0, new_script_size);
        {
            TCHAR self_path[32768];
            memset(self_path, 0, sizeof(self_path));
            if (!get_self_path(self_path, ARRAYSIZE(self_path)))
            {
                env->ThrowError("%s: Unable to get path of self.", params->filter_name);
            }
            if (!append_platform_if_needed(params->slave_platform, self_path, ARRAYSIZE(self_path) - 1))
            {
                env->ThrowError("%s: Unexpected error: Unable to convert platform name.", params->filter_name);
            }
            sprintf(new_script, "LoadPlugin(\"" PRINTF_TSTR "\")\n", self_path);
        }
        if (params->source_port > 0)
        {
            sprintf(buffer, TCPSOURCE_TEMPLATE "\n", params->source_port);
            strcat(new_script, buffer);
        }
        strcat(new_script, params->script);
        strcat(new_script, "\n");
        int port = get_unused_port();
        if (port <= 0)
        {
            env->ThrowError("%s: Unable to get unused port, code: %d", params->filter_name, port);
        }
        sprintf(buffer, "TCPServer(%d)\n", port);
        strcat(new_script, buffer);

        char error_msg[1024];
        memset(error_msg, 0, sizeof(error_msg));

        slave_create_params new_params;
        memcpy(&new_params, params, sizeof(new_params));
        new_params.script = new_script;

        *slave_stdin_handle = create_slave_process(&new_params, error_msg, sizeof(error_msg));
        if (!*slave_stdin_handle)
        {
            char error_buffer[1536];
            memset(error_buffer, 0, sizeof(error_buffer));
            sprintf(error_buffer, "%s: Unable to create slave process. Message: %s", params->filter_name, error_msg);
            env->ThrowError(error_buffer);
        }

        *new_slave_port = port;
    }
    __finally
    {
        free(new_script);
    }
}