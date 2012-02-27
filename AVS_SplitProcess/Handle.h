#pragma once

#include <Windows.h>
#include <stdexcept>
#include <assert.h>
#include "NonCopyableClassBase.h"

class OwnedHandle : private NonCopyableClassBase
{
public:
    OwnedHandle(HANDLE handle, bool check_handle = false) : _handle(handle) 
    {
        if (check_handle)
        {
            check_valid();
        }
    }
    virtual ~OwnedHandle()
    {
        close();
    }

    void replace(HANDLE new_handle, bool check_handle = false)
    {
        close();
        _handle = new_handle;
        if (check_handle)
        {
            check_valid();
        }
    }

    HANDLE get() const
    {
        return _handle;
    }

    bool is_valid() const
    {
        return _handle != 0 && _handle != INVALID_HANDLE_VALUE;
    }

    void check_valid() const
    {
        if (!is_valid())
        {
            assert(false);
            throw std::runtime_error("Invalid handle.");
        }
    }

    DWORD wait(DWORD ms_timeout = INFINITE, BOOL apc_aware = FALSE) const
    {
        return WaitForSingleObjectEx(_handle, ms_timeout, apc_aware);
    }
protected:
    HANDLE _handle;

    void close()
    {
        CloseHandle(_handle);
        _handle = NULL;
    }
};

class OwnedEventHandle : public OwnedHandle
{
public:
    OwnedEventHandle(HANDLE handle, bool check_handle = false) : OwnedHandle(handle, check_handle) {};

    void set()
    {
        SetEvent(_handle);
    }

    void reset()
    {
        ResetEvent(_handle);
    }
};