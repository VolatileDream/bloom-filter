#include "BloomFilter.h"

#include <getopt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "./lib/MurmurHash3.h"

#define FILTER_MODE_REMOVE_DUP 1
#define FILTER_MODE_ONLY_DUPS 2
#define MAX_KEY_SIZE 8192

typedef bloom_filter::BloomFilter Bf;

void usage(char arg0[]) {
  std::cout << "usage: " << arg0 << std::endl
      << "  --false-positive|-p <0.0> : sets the false positive rate" << std::endl
      << "  --elements|-n <10> : sets the key insertion estimate" << std::endl
      << std::endl
      << std::endl
      << "  --load|-l <file> : tries to load the specified filter" << std::endl
      << "                     Can be specified any number of times." << std::endl
      << "                     Attempting to load incompatible filters" << std::endl
      << "                     will cause all but the first to be ignored." << std::endl
      << "  --save|-s <file> : saves the filter upon exiting the program." << std::endl
      << "                     (assuming no error occured)" << std::endl
      << "                     Can only be set once." << std::endl
      << std::endl
      << std::endl
      << "Set one of the following to enable stdin -> stdout filtering." << std::endl
      << "  --remove-duplicates|-r : runs the filter in a mode that removes duplicates." << std::endl
      << "  --only-duplicates|-d : runs the filter in a mode that keeps only duplicate items." << std::endl
      << std::endl
      << "  --no-update-filter|-u : insertions don't update the filter" << std::endl
      << "                          Most usefully combined with -d to keep only a preset set of keys" << std::endl
      << std::endl
      << "  --help|-h|-? : prints this usage message" << std::endl;
}

// -p|--false-positive d : false positive rate
// -n|--elements n : item estimate
//
// --load f : read from f (multiple times)
// --save file : output to file
//
// --remove-duplicates : allow only new items
// --only-duplicates : allow only items previously seen

void CoercedHashFn(const void * key, int len, uint32_t seed, uint128_t * out) {
  MurmurHash3_x64_128(key, len, seed, (void*)out);
}

class BloomFilterApp {
 public:
  BloomFilterApp() : bf_(nullptr), save_(nullptr), filterMode_(0) {}

  bool CreateFilter(char* fpRate, char* elements) {
    if (bf_) {
      return false;
    }
    double fp = strtod(fpRate, nullptr);
    uint64_t n = atol(elements);

    bf_ = bloom_filter::BloomFilterBuilder()
      .SetMaximumItemEstimate(n)
      .SetDesiredFalsePositiveRate(fp)
      .Build(CoercedHashFn);

    return true;
  }

  bool Load(char* opt) {
    std::fstream f(opt, std::fstream::in);
    Bf* loaded = ser_.ReadFromFile(f, CoercedHashFn);
    if (!loaded) {
      return false;
    }
    if (!bf_) {
      // Not already set.
      bf_ = loaded;
      return true;
    }

    Bf* next = bf_->Merge(loaded);
    if (!next) {
      std::cerr << "failed to merge filters" << std::endl;
      return false;
    }
    std::swap(next, bf_);
    // Merge creates a new ptr
    delete next;
    delete loaded;
    return true;
  }

  bool QueueSave(char* opt) {
    if (save_) {
      return false;
    }
    save_ = opt;
    return true;
  }

  bool MaybeSaveOnExit() {
    if (!save_) {
      return true;
    }
    std::fstream f(save_, std::fstream::out);
    return ser_.WriteToFile(f, bf_);
  }

  bool QueueFilter(bool removeDups) {
    if (filterMode_ != 0) {
      return false;
    }
    if (removeDups) {
      filterMode_ = FILTER_MODE_REMOVE_DUP;
    } else {
      filterMode_ = FILTER_MODE_ONLY_DUPS;
    }
    return true;
  }

  void Filter(std::istream& i, std::ostream& o, bool update = true) {
    if (!filterMode_) {
      // filtering not set
      return;
    }
    std::string buffer;
    while (i.good()) {
      buffer.clear();
      std::getline(i, buffer);

      if (buffer.size() == 0) { continue; }

      bool in_filter = false;
      if (update) {
        in_filter = bf_->Add(buffer.c_str(), buffer.size());
      } else {
        in_filter = bf_->Check(buffer.c_str(), buffer.size());
      }

      //std::cerr << "(" << buffer << "): " << in_filter << std::endl;

      if ((filterMode_ == FILTER_MODE_REMOVE_DUP && !in_filter)
          || (filterMode_ == FILTER_MODE_ONLY_DUPS && in_filter)) {
        o << buffer << std::endl;
      }
    }
  }

 protected:
  bloom_filter::BloomFilterSerializer ser_;
  bloom_filter::BloomFilterBuilder builder_;
  Bf* bf_;
  char* save_;
  int filterMode_;
};

void ConsumeArgs(BloomFilterApp *app, int argc, char* argv[]) {
  struct option options[] = {
    // { char* name, has_arg, int*flag, int }, // short name
    // File io options
    { "save", required_argument, nullptr, 's' }, // s:
    { "load", required_argument, nullptr, 'l' }, // l:

    // Filtering options
    { "remove-duplicates", no_argument, nullptr, 'r' }, // r
    { "only-duplicates", no_argument, nullptr, 'd' }, // d
    { "no-update-filter", no_argument, nullptr, 'u' }, // u

    // Filter setup options
    { "false-positive", required_argument, 0, 'p' }, // p:
    { "elements", required_argument, 0, 'n' }, // n:

    // Funny options
    { "help", no_argument, 0, 'h' }, // h

    { nullptr, 0, nullptr, 0 },
  };

  char* fpRate = nullptr;
  char* elements = nullptr;
  bool update = true;

  while(true) {
    const int c = getopt_long(argc, argv, "s:l:p:n:rduh", options, nullptr);
    if (c == -1) {
      break;
    }

    switch(c) {
     case 's':
      if (!app->QueueSave(optarg)) {
        std::cerr << "--save|-s specified more than once!" << std::endl;
        exit(2);
      }
      break;
     case 'l':
      if (!app->Load(optarg)) {
        std::cerr << "unable to load file: " << optarg << std::endl;
        exit(3);
      }
      break;
     case 'r':
     case 'd':
      if (!app->QueueFilter(c == 'r')) {
        std::cerr << "duplicate filtering already set, bad flag: " << argv[optind - 1] << std::endl;
        exit(4);
      }
      break;
     case 'p':
      if (fpRate) {
        std::cerr << "false positive rate already set: " << fpRate << std::endl;
        exit(5);
      }
      fpRate = optarg;
      break;
     case 'n':
      if (elements) {
        std::cerr << "number of elements already set: " << elements << std::endl;
        exit(6);
      }
      elements = optarg;
      break;
     case 'u':
      update = !update;
      break;
     case '?':
     case 'h':
      usage(argv[0]);
      exit(1);
    }
  }

  // Attempt to create new filter.
  if (fpRate || elements) {
    if (fpRate && elements) {
      app->CreateFilter(fpRate, elements);
    } else {
      std::cerr << "not all args provided to create filter (false-positive-rate, number of elements) = "
          << "(" << fpRate << ", " << elements << ")" << std::endl;
      exit(7);
    }
  }

  app->Filter(std::cin, std::cout, update);

  if(!app->MaybeSaveOnExit()) {
    std::cerr << "error saving filter" << std::endl;
    exit(8);
  }
}

int main(int argc, char* argv[]) {
  BloomFilterApp app;
  ConsumeArgs(&app, argc, argv);
}
