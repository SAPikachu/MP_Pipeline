#pragma once

#if defined(_M_IX86) || defined(_M_AMD64)
#define HAS_ASMLIB
#endif

#ifdef HAS_ASMLIB

// ensure Windows.h is loaded first
#include <Windows.h>
#include <memory.h>
#include <string.h>

#include "asmlib.h"

#define memcpy A_memcpy
#define memset A_memset
#define memmove A_memmove
#define strcpy A_strcpy
#define strcmp A_strcmp
#define stricmp A_stricmp
#define strlen A_strlen

#endif