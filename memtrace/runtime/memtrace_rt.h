#pragma once
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

void logAccess(void* address, size_t size, int type);
void analyzeAndPrint();

#ifdef __cplusplus
}
#endif
