#ifndef min_common_h
#define min_common_h
#define _CRT_SECURE_NO_WARNINGS

#define DEBUG_TRACE_EXECUTION
#define DEBUG_PRINT_CODE
#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC

#define UINT8_COUNT (255 + 1)
#define UINT16_T_MAX 0xFFFF
#define UINT8_T_MAX 0xFF
#define DATE_FMT "%c"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void markCompilerRoots();
extern const char** _args;
extern int _argc;

#endif
