#pragma once

// workaround for mixed char & wchar_t environment


#ifdef UNICODE
#define tstring wstring
#else
#define tstring string
#endif

typedef wchar_t SYSCHAR;
#define sys_string std::wstring