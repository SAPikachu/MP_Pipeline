#pragma once

#include <Windows.h>

class OwnedHandle
{
public:
    OwnedHandle(HANDLE handle) : _handle(handle) {};
    ~OwnedHandle()
    {
        close();
    }

    void replace(HANDLE new_handle)
    {
        close();
        _handle = new_handle;
    }

    HANDLE get() const
    {
        return _handle;
    }

    bool is_valid() const
    {
        return _handle != 0 && _handle != INVALID_HANDLE_VALUE;
    }

    DWORD wait(DWORD ms_timeout = INFINITE) const
    {
        return WaitForSingleObject(_handle, ms_timeout);
    }
private:
    HANDLE _handle;

    void close()
    {
        CloseHandle(_handle);
        _handle = NULL;
    }
    
    OwnedHandle(const OwnedHandle&);
    OwnedHandle& operator=(const OwnedHandle&);
};