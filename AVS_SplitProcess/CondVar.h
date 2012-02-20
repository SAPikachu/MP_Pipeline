#pragma once

#include "SpinLock.h"
#include "Handle.h"
#include "SystemChar.h"

#include <Windows.h>

class CondVar
{
public:
    CondVar(volatile spin_lock_value_type_t* lock_ptr, const sys_string& event_name, BOOL is_auto_reset);
    
    SpinLock<> lock;
    OwnedEventHandle signal;
};