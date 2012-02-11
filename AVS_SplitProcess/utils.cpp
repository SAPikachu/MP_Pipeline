#define _CRT_NON_CONFORMING_SWPRINTFS

#include "stdafx.h"
#include "utils.h"

int sprintf_append(char* buffer, const char* format, ...)
{
    size_t len = strlen(buffer);
    va_list args;
    va_start(args, format);
    return vsprintf(buffer + len, format, args);
}

int wsprintf_append(wchar_t* buffer, const wchar_t* format, ...)
{
    size_t len = wcslen(buffer);
    va_list args;
    va_start(args, format);
    return _vswprintf(buffer + len, format, args);
}


bool get_self_path_a(char* buffer, DWORD buffer_size)
{
    HMODULE module = NULL;
    if (!GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPTSTR)get_self_path,
        &module))
    {
        return false;
    }
    buffer[0] = NULL;
    buffer[buffer_size - 1] = NULL;
    return !!GetModuleFileNameA(module, buffer, buffer_size - 1);
}

bool get_self_path_w(wchar_t* buffer, DWORD buffer_size)
{
    HMODULE module = NULL;
    if (!GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPTSTR)get_self_path,
        &module))
    {
        return false;
    }
    buffer[0] = NULL;
    buffer[buffer_size - 1] = NULL;
    return !!GetModuleFileNameW(module, buffer, buffer_size - 1);
}

__int64 us_time()
{
    LARGE_INTEGER ticks, ticksPerSecond;
    QueryPerformanceFrequency(&ticksPerSecond);
    QueryPerformanceCounter(&ticks);

    return ticks.QuadPart * 1000 * 1000 / ticksPerSecond.QuadPart;
}