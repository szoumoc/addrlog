#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif



void memtrace_log(void* address, size_t size, int type);


void memtrace_dump();

#ifdef __cplusplus
}
#endif
