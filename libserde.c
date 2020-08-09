#include "libserde.h"

// Handle endian conversion.
#include <byteswap.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
// Little Endian
#define htobe64(x) bswap_64(x)
#define htobe32(x) bswap_32(x)
#define be32toh(x) bswap_32(x)
#define be64toh(x) bswap_64(x)

#else
// Big Endian
#define htobe64(x) (x)
#define htobe32(x) (x)
#define be32toh(x) (x)
#define be64toh(x) (x)

#endif /* __BYTE_ORDER == __LITTLE_ENDIAN */

bool out32(FILE *f, uint32_t i) {
  i = htobe32(i);
  return fwrite(&i, sizeof(uint32_t), 1, f) != 1;
}
bool out64(FILE *f, uint64_t i) {
  i = htobe64(i);
  return fwrite(&i, sizeof(uint64_t), 1, f) != 1;
}

bool read32(FILE *f, uint32_t *i) {
  size_t s = fread(i, sizeof(uint32_t), 1, f);
  *i = be32toh(*i);
  return s != 1;
}
bool read64(FILE *f, uint64_t *i) {
  size_t s = fread(i, sizeof(uint64_t), 1, f);
  *i = be64toh(*i);
  return s != 1;
}

