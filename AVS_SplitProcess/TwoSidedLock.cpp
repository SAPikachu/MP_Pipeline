#define UNICODE
#define _UNICODE

#include "stdafx.h"
#include "TwoSidedLock.h"
#include <string>
#include <stdexcept>
#include <assert.h>
#include <sstream>
#include <stdio.h>

using namespace std;

#ifdef UNICODE
#define tstring wstring
#else
#define tstring string
#endif

#define LOCK_NAME_SUFFIX_SERVER TEXT("_Server")
#define LOCK_NAME_SUFFIX_CLIENT TEXT("_Client")

TwoSidedLock::TwoSidedLock(const SYSCHAR* lock_prefix, const SYSCHAR* lock_name,  bool is_server_side) :
    _event_this_side(NULL),
    _event_other_side(NULL)
{
    SYSCHAR buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    if (_sntprintf(buffer, ARRAYSIZE(buffer) - 1, TEXT("%s_%s"), lock_prefix, lock_name) < 0)
    {
        throw invalid_argument("lock_prefix and lock_name are too long.");
    }
    if (is_server_side)
    {
        create_server(buffer);
    } else {
        create_client(buffer);
    }
}

void TwoSidedLock::switch_to_other_side()
{
    ResetEvent(_event_this_side.get());
    SetEvent(_event_other_side.get());
}

bool TwoSidedLock::wait_on_this_side(DWORD ms_timeout)
{
    DWORD result = _event_this_side.wait(ms_timeout);
    switch (result)
    {
    case WAIT_OBJECT_0:
        return true;
        break;
    case WAIT_TIMEOUT:
        return false;
        break;
    default:
        ostringstream error_msg;
        error_msg << "Error when waiting on the event, return value: " << result << " error code: " << GetLastError();
        throw runtime_error(error_msg.str());
    }
}

void TwoSidedLock::signal_all()
{
    SetEvent(_event_this_side.get());
    SetEvent(_event_other_side.get());
}

void check_event(OwnedHandle& handle)
{
    if (!handle.is_valid())
    {
        throw runtime_error("Unable to create event.");
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        throw runtime_error("The event already exists.");
    }
}

void TwoSidedLock::create_server(const SYSCHAR* lock_name)
{
    tstring event_name(lock_name);
    event_name.append(LOCK_NAME_SUFFIX_SERVER);
    _event_this_side.replace(CreateEvent(NULL, FALSE, TRUE, event_name.c_str()));
    check_event(_event_this_side);
    
    event_name.assign(lock_name);
    event_name.append(LOCK_NAME_SUFFIX_CLIENT);
    _event_other_side.replace(CreateEvent(NULL, FALSE, FALSE, event_name.c_str()));
    check_event(_event_other_side);
}

void TwoSidedLock::create_client(const SYSCHAR* lock_name)
{
    // don't know whether OpenEvent will reset the last error code, reset it here for safe
    SetLastError(ERROR_SUCCESS);

    tstring event_name(lock_name);
    event_name.append(LOCK_NAME_SUFFIX_CLIENT);
    _event_this_side.replace(OpenEvent(NULL, FALSE, event_name.c_str()));
    check_event(_event_this_side);
    
    event_name.assign(lock_name);
    event_name.append(LOCK_NAME_SUFFIX_SERVER);
    _event_other_side.replace(OpenEvent(NULL, FALSE, event_name.c_str()));
    check_event(_event_other_side);
}

TwoSidedLockAcquire::TwoSidedLockAcquire(TwoSidedLock& lock) :
    _lock(lock)
{
    _lock.wait_on_this_side(INFINITE);
}

TwoSidedLockAcquire::~TwoSidedLockAcquire()
{
    _lock.switch_to_other_side();
}