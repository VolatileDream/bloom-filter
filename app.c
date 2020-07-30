#include "app.h"

#include <stdlib.h>
#include <string.h>

#define MAX_KEY_LEN 4096

enum filter_mode {
  Unset = 0,
  RemoveDuplicates = 1,
  OnlyDuplicates = 2,
};
struct bloom_app {
  hash_func func;
  filter_t *filter;
  char *save;
  enum filter_mode mode;
};

// Require opening a file.
FILE* req_open(const char *file, const char *mode) {
  FILE *f = fopen(file, mode);
  if (!f) {
    char buffer[1024] = {0};
    sprintf(buffer, "Error occurred with open(%s,%s): ", file, mode);
    perror(buffer);
    exit(1);
  }
  return f;
}

app_t* app_init(hash_func func) {
  app_t *res = (app_t*) malloc(sizeof(app_t));
  res->filter = 0;
  res->save = 0;
  res->mode = Unset;
  res->func = func;
  return res;
}
void app_del(app_t* a) {
  if (a->filter) {
    bf_del(a->filter);
  }
  free(a);
}

bool app_create_filter(app_t *a, char *fpRate, char *elements) {
  if (a->filter) {
    return false;
  }
  double fp = strtod(fpRate, 0);
  uint64_t n = atol(elements);

  a->filter = bf_init(n, fp, a->func);

  return true;
}

int app_load(app_t *a, char *file) {
  FILE *input = req_open(file, "r");
  filter_t *fl = bf_read_from_file(input, a->func);
  fclose(input);
  if (!fl) {
    return 1;
  }

  if (!a->filter) {
    a->filter = fl;
    return 0;
  }

  filter_t *next = bf_merge(a->filter, fl);
  if (!next) {
    return 2;
  }

  bf_del(a->filter);
  bf_del(fl);
  a->filter = next;
  return 0;
}
bool app_queue_save(app_t *a, char *file) {
  if (a->save) {
    return false;
  }
  a->save = file;
  return true;
}
bool app_maybe_save_on_exit(app_t *a) {
  if (!a->save) {
    return true;
  }
  FILE *output = req_open(a->save, "w");
  bool res = bf_write_to_file(a->filter, output);
  fclose(output);
  return res;
}

bool app_queue_filter(app_t *a, bool remove_duplicates) {
  if (a->mode) {
    return false;
  }
  a->mode = remove_duplicates ? RemoveDuplicates : OnlyDuplicates;
  return true;
}

bool read_key(FILE *in, char *buf, uint32_t length, uint32_t *read) {
  uint32_t len = 0;
  while (len < length) {
    int c = fgetc(in);
    if (c == EOF) {
      return false;
    } else if (c == '\n') {
      break;
    }
    buf[len] = (char)c;
    len++;
  }

  *read = len;
  if (len >= length) {
    // consume the rest of the line.
    int c = 0;
    while (c != '\n' && c != EOF) {
      c = fgetc(in);
    }
  }
  return true;
}

void app_filter(app_t *a, FILE *in, FILE *out, bool update) {
  if (a->mode == Unset || !a->filter) {
    return;
  }

  char buffer[MAX_KEY_LEN + 1] = {0};
  uint32_t length = 0;

  while (read_key(in, buffer, MAX_KEY_LEN, &length)) {
    buffer[length] = '\n';
    bool exists = false;
    if (update) {
      exists = bf_add(a->filter, buffer, length);
    } else {
      exists = bf_has(a->filter, buffer, length);
    }
    if ((a->mode == RemoveDuplicates && !exists)
        || (a->mode == OnlyDuplicates && exists)) {
      fwrite(buffer, sizeof(char), length + 1, out);
    }
  }
}

