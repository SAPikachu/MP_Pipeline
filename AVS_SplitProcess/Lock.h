#pragma once

#include <Windows.h>

class CriticalSectionLock
{
public:
    CriticalSectionLock();
    ~CriticalSectionLock();

    void Lock();
    void Unlock();
#ifdef _DEBUG
    bool IsOwnedLock();
#endif

private:
    CRITICAL_SECTION _section;
#ifdef _DEBUG
    DWORD _owning_thread_id;
    DWORD _enter_time;
#endif
    
    CriticalSectionLock(const CriticalSectionLock&);
    CriticalSectionLock& operator=(const CriticalSectionLock&);
};

class CSLockAcquire
{
public:
    CSLockAcquire(CriticalSectionLock& lock) : _lock(lock)
    {
        _lock.Lock();
    }
    ~CSLockAcquire()
    {
        _lock.Unlock();
    }

private:
    CriticalSectionLock& _lock;

    CSLockAcquire(const CSLockAcquire&);
    CSLockAcquire& operator=(const CSLockAcquire&);
};