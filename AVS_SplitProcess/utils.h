#pragma once

int sprintf_append(char* buffer, const char* format, ...);
int wsprintf_append(wchar_t* buffer, const wchar_t* format, ...);

bool get_self_path_a(char* buffer, DWORD buffer_size);
bool get_self_path_w(wchar_t* buffer, DWORD buffer_size);

#ifdef UNICODE
#define tprintf_append wsprintf_append
#define get_self_path get_self_path_w
#else
#define tprintf_append sprintf_append
#define get_self_path get_self_path_a
#endif