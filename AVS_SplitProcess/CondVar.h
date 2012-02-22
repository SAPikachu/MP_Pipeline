#pragma once

#include "SpinLock.h"
#include "TwoSidedLock.h"
#include "SystemChar.h"
#include "NonCopyableClassBase.h"

#include <Windows.h>

class CondVar : NonCopyableClassBase
{
public:
    CondVar(volatile spin_lock_value_type_t* lock_ptr, const sys_string& event_name, bool is_server);
    void lock_short(); // expect the lock won't be held for a long time
    bool lock_long(unsigned timeout = INFINITE); // the lock may be held for a long time
    
    SpinLock<> lock;
    TwoSidedLock signal;
};

class CondVarLockAcquire : NonCopyableClassBase
{
public:
    static const bool LOCK_SHORT = true;
    static const bool LOCK_LONG = false;

    bool signal_after_unlock;

    CondVarLockAcquire(CondVar& cond, bool lock_type);
    ~CondVarLockAcquire();
private:
    CondVar& _cond;
};
