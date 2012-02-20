#pragma once

#include <intrin.h>
#include <assert.h>

typedef long spin_lock_value_type_t;
static const spin_lock_value_type_t UNLOCKED = 0;

template <spin_lock_value_type_t lock_value = 1>
class SpinLockImplementationBasic
{
    static_assert(lock_value != UNLOCKED, "lock_value must NOT be equal to UNLOCKED");

public:
    SpinLockImplementationBasic(volatile spin_lock_value_type_t* lock_ptr) : _lock_ptr(lock_ptr) {}
    void on_before_locking() {}
    int try_lock_once()
    {
        return _InterlockedCompareExchange(_lock_ptr, lock_value, UNLOCKED) == UNLOCKED;
    }
    void unlock()
    {
        assert(*_lock_ptr == lock_value);
        // InterlockedExchange(_lock_ptr, UNLOCKED);
        // since we don't care about the return value, and VC guarentees RELEASE semantic on VOLATILE WRITE operations, it is safe to just assign to the variable here
        // references:
        // http://msdn.microsoft.com/en-us/library/12a04hfd.aspx
        // http://blogs.msdn.com/b/kangsu/archive/2007/07/16/volatile-acquire-release-memory-fences-and-vc2005.aspx
        // http://groups.google.com/group/lock-free/browse_thread/thread/8fd4c43404d3882e
        *_lock_ptr = UNLOCKED;
    }
private:
    volatile spin_lock_value_type_t* _lock_ptr;
};


class SpinLockImplementationDebug
{
public:
    SpinLockImplementationDebug(volatile spin_lock_value_type_t* lock_ptr) : _lock_ptr(lock_ptr) {}
    void on_before_locking() 
    {
        assert(*_lock_ptr != get_lock_value());
    }
    int try_lock_once()
    {
        return _InterlockedCompareExchange(_lock_ptr, get_lock_value(), UNLOCKED) == UNLOCKED;
    }
    void unlock()
    {
        assert(*_lock_ptr == get_lock_value());
        // InterlockedExchange(_lock_ptr, UNLOCKED);
        // since we don't care about the return value, and VC guarentees RELEASE semantic on VOLATILE WRITE operations, it is safe to just assign to the variable here
        // references:
        // http://msdn.microsoft.com/en-us/library/12a04hfd.aspx
        // http://blogs.msdn.com/b/kangsu/archive/2007/07/16/volatile-acquire-release-memory-fences-and-vc2005.aspx
        // http://groups.google.com/group/lock-free/browse_thread/thread/8fd4c43404d3882e
        *_lock_ptr = UNLOCKED;
    }
private:
    spin_lock_value_type_t __inline get_lock_value()
    {
        return (spin_lock_value_type_t)GetCurrentThreadId();
    }
    volatile spin_lock_value_type_t* _lock_ptr;
};
