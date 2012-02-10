#include "stdafx.h"
#include "Lock.h"
#include <assert.h>

CriticalSectionLock::CriticalSectionLock()
{
    memset(&_section, 0, sizeof(_section));
    InitializeCriticalSection(&_section);
}

CriticalSectionLock::~CriticalSectionLock()
{
    DeleteCriticalSection(&_section);
}

void CriticalSectionLock::Lock()
{
    EnterCriticalSection(&_section);
#ifdef _DEBUG
    _enter_time = GetTickCount();
#endif
}

void CriticalSectionLock::Unlock()
{
#ifdef _DEBUG
    if (GetTickCount() - _enter_time > 1000)
    {
        assert(("The locking period is too long!", false));
    }
#endif
    LeaveCriticalSection(&_section);
}