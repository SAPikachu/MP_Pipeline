
#include <Windows.h>

#define SCRIPT_VAR_NAME_A "SPLITPROCESS_SCRIPT"
#define SCRIPT_VAR_NAME_U L ## SCRIPT_VAR_NAME_A

#ifdef UNICODE
#define SCRIPT_VAR_NAME SCRIPT_VAR_NAME_U
#else
#define SCRIPT_VAR_NAME SCRIPT_VAR_NAME_A
#endif

#define SLAVE_OK_FLAG "SLAVE_OK"