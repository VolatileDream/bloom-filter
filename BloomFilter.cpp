#include "BloomFilter.h"

#include <cassert>
#include <cmath>
#include <new>

// For byte order functions.
#include <endian.h>

namespace bloom_filter {

BloomFilterBuilder& BloomFilterBuilder::SetMaximumItemEstimate(uint64_t e) {
  itemEstimate_ = e;
  return *this;
}

BloomFilterBuilder& BloomFilterBuilder::SetDesiredFalsePositiveRate(double r) {
  desiredFalsePositiveRate_ = r;
  return *this;
}

BloomFilter* BloomFilterBuilder::Build(hash_func h) {
  if (itemEstimate_ <= 0 || desiredFalsePositiveRate_ <= 0.0) {
    return nullptr;
  }
  // As per the wikipedia page, the math here is approximate.

  // hashes = -log2(p)
  uint32_t hashes = ceil(-log2(desiredFalsePositiveRate_));
  // total bits = -log2(p) * elements / ln(2)
  size_t number_of_bits = ceil(hashes * itemEstimate_ / log(2.0L));
  return new BloomFilter(hashes, h, number_of_bits);
}

BloomFilter::BloomFilter()
 : hashCount_(0), func_(nullptr), size_(), contents_() {}

BloomFilter::BloomFilter(uint32_t hashCount, hash_func func, size_t size)
 : hashCount_(hashCount), func_(func), size_(size), contents_(std::vector<bool>(size_)) {}

BloomFilter* BloomFilter::Merge(BloomFilter* other) const {
  // Invalid arguments
  if (size_ != other->size_ || hashCount_ != other->hashCount_) {
    return nullptr;
  }

  BloomFilter* bf = new BloomFilter(hashCount_, func_, size_);
  for (uint64_t i = 0; i < size_; i++) {
    bf->contents_[i] = contents_[i] || other->contents_[i];
  }

  return bf;
}

bool BloomFilter::IsSet(size_t index) const {
  return contents_[index];
}

bool BloomFilter::Set(size_t index, bool val) {
  bool prev = contents_[index];
  contents_[index] = val;
  return prev != val;
}

bool BloomFilter::Add(const void * key, int len) {
  bool changed = false;
  for (uint32_t idx = 0; idx < hashCount_; idx++) {
    uint32_t hash;
    (*func_)(key, len, idx, &hash);
    size_t index = hash % size_;
    changed |= Set(index, true);
  }
  return !changed;
}
bool BloomFilter::Check(const void * key, int len) const {
  bool present = true;
  for (uint32_t idx = 0; idx < hashCount_; idx++) {
    uint32_t hash;
    (*func_)(key, len, idx, &hash);
    size_t index = hash % size_;
    present &= IsSet(index);
  }
  return present;
}

uint32_t read32(std::istream& f) {
  uint32_t i;
  f.read((char*)&i, sizeof(uint32_t));
  return be32toh(i);
}
uint64_t read64(std::istream& f) {
  uint64_t i;
  f.read((char*)&i, sizeof(uint64_t));
  return be64toh(i);
}

BloomFilter* BloomFilterSerializer::ReadFromFile(std::istream& f, hash_func h) {
  uint32_t header = read32(f);
  uint32_t hashes = read32(f);
  size_t size = read64(f);

  if (header != MAGIC_HEADER) {
    std::cerr << "magic header didn't match!" << std::endl 
      << "found: " << std::hex << header << "wanted: " << MAGIC_HEADER;
    return nullptr;
  }
  BloomFilter * bf = new BloomFilter();
  bf->hashCount_ = hashes;
  bf->size_ = size;
  bf->func_ = h;
  // size is in bits, we need to read uint64_t
  for (size_t i = 0; i < size; i += 64) {
    uint64_t chunk = read64(f);
    PushBack(bf->contents_, chunk);
  }

  if (f.fail()) {
    std::cerr << "failed to deserialize content" << std::endl;
    return nullptr;
  }
  return bf;
}

void out32(std::ostream& f, uint32_t i) {
  i = htobe32(i);
  f.write((char*)&i, sizeof(uint32_t));
}
void out64(std::ostream& f, uint64_t i) {
  i = htobe64(i);
  f.write((char*)&i, sizeof(uint64_t));
}

bool BloomFilterSerializer::WriteToFile(std::ostream& f, BloomFilter * bf) {
  out32(f, MAGIC_HEADER);
  out32(f, bf->hashCount_);
  out64(f, bf->size_);
  std::vector<bool> copy = bf->contents_;
  PushBack(copy, 0); // ensure that the last read gets a whole 64 bits.
  for (size_t i = 0; i < bf->size_; i += 64) {
    out64(f, ReadOutOffset(copy, i));
  }

  if (f.fail()) {
    std::cerr << "failed to write out filter" << std::endl;
    return false;
  }
  return true;
}

// Slow... :c
uint64_t BloomFilterSerializer::ReadOutOffset(std::vector<bool>& c, size_t offset) {
  uint64_t out = 0;
  for (int i = 0; i < 64; i++) {
    out = out << 1 | c[offset + i];
  }
  return out;
}
void BloomFilterSerializer::PushBack(std::vector<bool>& c, uint64_t d) {
  for (int idx = 63; idx >= 0; idx--) {
    bool insert = (d >> idx) & 0x1;
    c.push_back(insert);
  }
}

}  // namespace bloom_filter
