#ifndef BLOOM_FILTER
#define BLOOM_FILTER

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <limits>
#include <vector>

namespace bloom_filter {

// Forward declare.
class BloomFilter;
class BloomFilterSerializer;

// Hash function filter.
typedef void (*hash_func)(const void * key, int len, uint32_t seed, void * out);

// Trivial builder class for BloomFilter.
// Allocates memory as necessary.
class BloomFilterBuilder {
 public:
  BloomFilterBuilder& SetMaximumItemEstimate(uint64_t e);
  // Rate must be between (0, 1)
  BloomFilterBuilder& SetDesiredFalsePositiveRate(double r);
  // Rate must be between (1, max).
  // Equivalent to SetDesiredFalsePositiveRate(1.0/r)
  BloomFilterBuilder& SetInverseDesiredFalsePositiveRate(double r) {
    return SetDesiredFalsePositiveRate(1.0/r);
  };
  BloomFilter* Build(hash_func h);

 protected:
  uint64_t itemEstimate_;
  double desiredFalsePositiveRate_;
  hash_func hash_;
};

class BloomFilter {
 public:
  // Creates an uninitalized bloom filter
  BloomFilter();
  // Creates a new filter given the parameters, and hash function.
  // Takes ownership of the contents pointer.
  BloomFilter(uint32_t hashCount, hash_func func, size_t size);

  // Remove the move & copy constructors.
  BloomFilter(const BloomFilter&) = delete;
  BloomFilter(BloomFilter&&) = delete;

  // Adds the item to the filter, returns true if the item was already present.
  bool Add(const void * key, int len);

  // Checks an item, returns true if it was already present.
  bool Check(const void * key, int len) const;

  // Merges another bloom filter with this one, returning a new filter.
  BloomFilter* Merge(BloomFilter * other) const;

 protected:
  // Fetches the value of of the index
  bool IsSet(size_t index) const;
  // Sets the value, returns true if it's changed.
  bool Set(size_t index, bool val);

 protected:
  uint32_t hashCount_;
  hash_func func_;
  size_t size_;
  std::vector<bool> contents_;

  friend class BloomFilterSerializer;
};

class BloomFilterSerializer {
 public:
  BloomFilter* ReadFromFile(std::istream& f, hash_func h);
  bool WriteToFile(std::ostream& f, BloomFilter* bf);
 protected:
  uint64_t ReadOutOffset(std::vector<bool>& c, size_t offset);
  void PushBack(std::vector<bool>& c, uint64_t d);
 private:
  static const uint32_t MAGIC_HEADER = 0xb100f11e;
};

}  // namespace bloom_filter

#endif /* BLOOM_FILTER */
