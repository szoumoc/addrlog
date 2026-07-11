#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called by injected instrumentation for every memory access
// type: 0 = READ, 1 = WRITE
void memtrace_log(void* address, size_t size, int type);

// Call at end of program to dump/analyze the log
void memtrace_dump();

#ifdef __cplusplus
}
#endif