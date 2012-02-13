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
    bool wait_on_this_side(DWORD ms_timeout);

private:
    OwnedHandle _event_this_side, _event_other_side;

    TwoSidedLock(const TwoSidedLock&);
    
    void create_server(const SYSCHAR* lock_name);
    void create_client(const SYSCHAR* lock_name);
};

class TwoSidedLockAcquire : private NonCopyableClassBase
{
public:
    TwoSidedLockAcquire(TwoSidedLock& lock);
    ~TwoSidedLockAcquire();
private:
    TwoSidedLock& _lock;
};
