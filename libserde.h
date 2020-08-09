#ifndef __LIB_SERDE__
#define __LIB_SERDE__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool out32(FILE *f, uint32_t i);
bool out64(FILE *f, uint64_t i);
bool read32(FILE *f, uint32_t *i);
bool read64(FILE *f, uint64_t *i);

#endif /* __LIB_SERDE__ */
