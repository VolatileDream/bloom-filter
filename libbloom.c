#include "libbloom.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define WORD_BIT_SIZE 64

static const uint32_t MAGIC_HEADER = 0xb100f11e;

struct bloom_filter {
  hash_func func; // hash function pointer.
  uint32_t hashes; // number of hashes to use.
  uint64_t size; // size of filter in bits.
  uint64_t *content; // pointer to memory 
};


uint64_t word_length(uint64_t bits) {
  uint64_t l = bits / WORD_BIT_SIZE;
  return l + (bits % WORD_BIT_SIZE > 0);
}

filter_t* bf_mk(hash_func hf, uint32_t hashes, uint64_t number_of_bits) {
  filter_t *f = (filter_t*)
      malloc(sizeof(filter_t) + word_length(number_of_bits)*sizeof(uint64_t));
  f->func = hf;
  f->hashes = hashes;
  f->size = number_of_bits;
  f->content = (uint64_t*)(f + 1);
  memset(f->content, 0, word_length(f->size)*sizeof(uint64_t));
  return f;
}

filter_t* bf_init(uint64_t max_items, double fp_rate, hash_func hf) {
  // hashes = -log2(p)
  uint32_t hashes = ceil(-log2(fp_rate));
  // total bits = -log2(p) * elements / ln(2)
  size_t number_of_bits = ceil(hashes * max_items / log(2.0L));

  filter_t *f = bf_mk(hf, hashes, number_of_bits);
  return f;
}

void bf_del(filter_t *f) {
  free(f);
}

bool bf_bit_is_set(filter_t *f, uint64_t bit) {
  uint64_t word = bit / WORD_BIT_SIZE;
  bit = bit % WORD_BIT_SIZE;
  return (f->content[word] >> bit) & 0x1;
}
bool bf_bit_set(filter_t *f, uint64_t bit) {
  uint64_t word = bit / WORD_BIT_SIZE;
  bit = bit % WORD_BIT_SIZE;
  uint64_t prev = f->content[word];
  f->content[word] |= 0x1 << bit;
  return prev == f->content[word];
}

bool bf_add(filter_t *f, const void *key, int len) {
  bool changed = false;
  for (uint32_t i = 0; i < f->hashes; i++) {
    uint32_t hash = f->func(i, key, len) % f->size;
    changed |= bf_bit_set(f, hash);
  }
  return changed;
}
bool bf_has(filter_t *f, const void *key, int len) {
  bool exists = true;
  for (uint32_t i = 0; i < f->hashes; i++) {
    uint32_t hash = f->func(i, key, len) % f->size;
    exists = exists && bf_bit_is_set(f, hash);
  }
  return exists;
}


filter_t* bf_merge(filter_t *f1, filter_t *f2) {
  if (f1->func != f2->func || f1->size != f2->size || f1->hashes != f2->hashes) {
    return false;
  }

  filter_t *res = bf_mk(f1->func, f1->hashes, f1->size);
  // Merge the content arrays.
  for (uint64_t i = 0; i < word_length(res->size); i++) {
    res->content[i] = f1->content[i] | f2->content[i];
  }
  return res;
}

bool out32(FILE *f, uint32_t i) {
  i = htobe32(i);
  return fwrite(&i, sizeof(uint32_t), 1, f) != 1;
}
bool out64(FILE *f, uint64_t i) {
  i = htobe64(i);
  return fwrite(&i, sizeof(uint64_t), 1, f) != 1;
}
bool bf_write_to_file(filter_t *f, FILE *file) {
  bool failure = false;
  failure = failure || out32(file, MAGIC_HEADER);
  failure = failure || out32(file, f->hashes);
  failure = failure || out64(file, f->size);
  for (uint64_t i=0; i < word_length(f->size) && !failure; i++) {
    failure = failure || out64(file, f->content[i]);
  }
  return !failure;
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

filter_t* bf_read_from_file(FILE *file, hash_func hf) {
  uint32_t header = 0;
  uint32_t hashes = 0;
  uint64_t size = 0;
  bool failure = false;
  failure = failure || read32(file, &header);
  failure = failure || read32(file, &hashes);
  failure = failure || read64(file, &size);

  if (failure || header != MAGIC_HEADER) {
    return 0;
  }

  filter_t *f = bf_mk(hf, hashes, size);
  for (uint64_t i=0; i < word_length(size) && !failure; i++) {
    failure = failure || read64(file, &f->content[i]);
  }

  if (failure) {
    bf_del(f);
    return 0;
  }
  return f;
}
