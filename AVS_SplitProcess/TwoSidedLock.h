#pragma once

#include <Windows.h>
#include "Handle.h"
#include "SystemChar.h"
#include "NonCopyableClassBase.h"

class TwoSidedLock : private NonCopyableClassBase
{
public:
    TwoSidedLock(const SYSCHAR* lock_prefix, const SYSCHAR* lock_name, bool is_server_side);

    void switch_to_other_side();
    void stay_on_this_side();
    bool wait_on_this_side(DWORD ms_timeout, BOOL apc_aware = FALSE);
    void signal_all();

    // note: don't close the returned handle, it will be closed by the destructor
    HANDLE get_event_this_side();

private:
    OwnedEventHandle _event_this_side, _event_other_side;

    TwoSidedLock(const TwoSidedLock&);
    
    void create_server(const SYSCHAR* lock_name);
    void create_client(const SYSCHAR* lock_name);
};

class TwoSidedLockContext : private NonCopyableClassBase
{
public:
    TwoSidedLockContext(TwoSidedLock& lock);
    ~TwoSidedLockContext();
protected:
    TwoSidedLock& _lock;
};

class TwoSidedLockAcquire : private TwoSidedLockContext
{
public:
    TwoSidedLockAcquire(TwoSidedLock& lock);
};