#ifndef _APP_H_
#define _APP_H_

#include "libbloom.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct bloom_app app_t;
struct bloom_app;

app_t* app_init();
void app_del(app_t* a);

bool app_create_filter(app_t *a, char *fp, char *elements);

int app_load(app_t *a, char *file);
bool app_queue_save(app_t *a, char *file);
bool app_maybe_save_on_exit(app_t *a);

bool app_queue_filter(app_t *a, bool remove_duplicates);
void app_filter(app_t *a, FILE *in, FILE *out, bool update);

#endif /* _APP_H_ */
