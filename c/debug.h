#pragma once

#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t getErrors();
int8_t show_err(int8_t rv, const char *msg);
void dumpBuffer(void *b, size_t l);
void print_backtrace(void);

#ifdef __cplusplus
}
#endif
