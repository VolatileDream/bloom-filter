#include "libbloom.h"
#include "app.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "third-party/smhasher/src/PMurHash.h"

void usage(char *arg0) {
  printf("usage: %s\n\n", arg0);
  printf("Create a new filter, not compatible with -l\n");
  printf("  --false-positive|-p <0.0> : sets the false positive rate\n");
  printf("  --elements|-n <10> : sets the key insertion estimate\n\n");
  printf("  --load|-l <file> : tries to load the specified filter\n");
  printf("                     Can be specified any number of times.\n");
  printf("                     Attempting to load incompatible filters\n");
  printf("                     will cause all but the first to be ignored.\n");
  printf("  --save|-s <file> : saves the filter upon exiting the program.\n");
  printf("                     (assuming no error occured)\n");
  printf("                     Can only be set once.\n\n");
  printf("Set one of the following to enable stdin -> stdout filtering.\n");
  printf("  --remove-duplicates|-r : runs the filter in a mode that removes duplicates.\n");
  printf("  --only-duplicates|-d : runs the filter in a mode that keeps only duplicate items.\n\n");
  printf("  --no-update-filter|-u : insertions don't update the filter\n");
  printf("                          Most usefully combined with -d to keep only a preset set of keys\n\n");
  printf("  --help|-h|-? : prints this usage message\n");
}

int run(app_t *app, int argc, char* argv[]) {
  struct option options[] = {
    // { char* name, has_arg, int*flag, int }, // short name
    // File io options
    { "save", required_argument, 0, 's' }, // s:
    { "load", required_argument, 0, 'l' }, // l:

    // Filtering options
    { "remove-duplicates", no_argument, 0, 'r' }, // r
    { "only-duplicates", no_argument, 0, 'd' }, // d
    { "no-update-filter", no_argument, 0, 'u' }, // u

    // Filter setup options
    { "false-positive", required_argument, 0, 'p' }, // p:
    { "elements", required_argument, 0, 'n' }, // n:

    // Funny options
    { "help", no_argument, 0, 'h' }, // h

    { 0, 0, 0, 0 },
  };

  char* fpRate = 0;
  char* elements = 0;
  bool update = true;

  while(true) {
    const int c = getopt_long(argc, argv, "s:l:p:n:rduh", options, 0);
    if (c == -1) {
      break;
    }

    switch(c) {
     case 's':
      if (!app_queue_save(app, optarg)) {
        fprintf(stderr, "--save|-s specified more than once!");
        return 2;
      }
      break;
     case 'l':
      if (app_load(app, optarg) != 0) {
        fprintf(stderr, "unable to load file: %s\n", optarg);
        return 3;
      }
      break;
     case 'r':
     case 'd':
      if (!app_queue_filter(app, c == 'r')) {
        fprintf(stderr, "duplicate filtering alread set, bad flag: %s\n", argv[optind - 1]);
        return 4;
      }
      break;
     case 'p':
      if (fpRate) {
        fprintf(stderr, "false positive rate already set: %s\n", fpRate);
        return 5;
      }
      fpRate = optarg;
      break;
     case 'n':
      if (elements) {
        fprintf(stderr, "number of elements already set: %s\n", elements);
        return 6;
      }
      elements = optarg;
      break;
     case 'u':
      update = !update;
      break;
     case '?':
     case 'h':
      usage(argv[0]);
      return 1;
    }
  }

  // Attempt to create new filter.
  if (fpRate || elements) {
    if (fpRate && elements) {
      if(!app_create_filter(app, fpRate, elements)) {
        fprintf(stderr, "error creating filter: was a filter already loaded?\n");
      }
    } else {
      fprintf(stderr,
          "not all args provided to create filter (false-positive-rate, number of elements) = (%s, %s)\n",
          fpRate,
          elements);
      return 7;
    }
  }

  app_filter(app, stdin, stdout, update);

  if(!app_maybe_save_on_exit(app)) {
    fprintf(stderr, "error saving filter\n");
    return 8;
  }

  return 0;
}

int main(int argc, char **argv) {
  app_t *app = app_init(PMurHash32);
  int rc = run(app, argc, argv);
  app_del(app);
  return rc;
}
