#define UNICODE
#define _UNICODE

#include "stdafx.h"
#include "CondVar.h"
#include <assert.h>

CondVar::CondVar(volatile spin_lock_value_type_t* lock_ptr, const sys_string& event_name, bool is_server) :
    lock(lock_ptr),
    signal(event_name.c_str(), SYSTEXT("Cond"), is_server)
{
}

void CondVar::lock_short()
{
    if (!lock.try_lock(SPIN_LOCK_UNIT * 5))
    {
        assert(("Someone has taken the request lock for a long time", false));
        while (!lock.try_sleep_lock(0x7fffffff))
        {
            // deadlock!
            assert(false);
        }
    }
}

bool CondVar::lock_long(unsigned timeout)
{
    while (true)
    {
        if (lock.try_lock(SPIN_LOCK_UNIT))
        {
            break;
        }
        if (lock.try_sleep_lock(SLEEP_LOCK_UNIT))
        {
            break;
        }
        if (!signal.wait_on_this_side(timeout))
        {
            return false;
        }
    }
    return true;
}

CondVarLockAcquire::CondVarLockAcquire(CondVar& cond, bool lock_type) :
    _cond(cond),
    signal_after_unlock(false)
{
    if (lock_type == LOCK_SHORT)
    {
        cond.lock_short();
    } else {
        cond.lock_long();
    }
}
CondVarLockAcquire::~CondVarLockAcquire()
{
    _cond.lock.unlock();
    if (signal_after_unlock)
    {
        _cond.signal.switch_to_other_side();
    }
}