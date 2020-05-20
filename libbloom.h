#ifndef LIB_BLOOM
#define LIB_BLOOM

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef uint32_t (*hash_func)(uint32_t seed, const void *key, int len);
typedef struct bloom_filter filter_t;

// Creates a new filter
filter_t* bf_init(uint64_t max_items, double fp_rate, hash_func hf);
// Deallocates the filter
void bf_del(filter_t *f);

// Adds a key to the filter, returns true if the key was already present.
bool bf_add(filter_t *f, const void *key, int len);
// Returns true if the key is present.
bool bf_has(filter_t *f, const void *key, int len);

// Initializes a new filter, merged from the previous two.
// Does not free the existing filters.
filter_t* bf_merge(filter_t *f1, filter_t *f2);

// Returns true on success.
bool bf_write_to_file(filter_t *f, FILE *file);
filter_t* bf_read_from_file(FILE *file, hash_func hf);

#endif /* LIB_BLOOM */
