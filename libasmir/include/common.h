#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#define LIBASMIR_VERSION "0.1"

typedef uint64_t address_t;

void panic(string msg);

#ifdef __cplusplus
}
#endif

#endif
