#define UNICODE
#define _UNICODE

#include "stdafx.h"
#include "CondVar.h"

CondVar::CondVar(volatile spin_lock_value_type_t* lock_ptr, const sys_string& event_name, BOOL is_manual_reset) :
    lock(lock_ptr),
    signal(CreateEvent(NULL, is_manual_reset, FALSE, event_name.c_str()), true)
{
}