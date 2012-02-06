#include "stdafx.h"
#include "Lock.h"

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
}

void CriticalSectionLock::Unlock()
{
    LeaveCriticalSection(&_section);
}