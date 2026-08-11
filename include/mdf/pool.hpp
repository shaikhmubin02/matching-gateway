#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace mdf {

template <typename T>
class ObjectPool {
public:
  explicit ObjectPool(std::size_t capacity) {
    storage_.resize(capacity);
    free_.reserve(capacity);
    for (std::size_t i = 0; i < capacity; ++i)
      free_.push_back(&storage_[i]);
  }

  T* acquire() {
    if (free_.empty())
      return nullptr;
    T* p = free_.back();
    free_.pop_back();
    return p;
  }

  void release(T* p) {
    if (!p)
      return;
    free_.push_back(p);
  }

  std::size_t available() const { return free_.size(); }
  std::size_t capacity() const { return storage_.size(); }

private:
  std::vector<T> storage_;
  std::vector<T*> free_;
};

class BufferArena {
public:
  BufferArena(std::size_t block_size, std::size_t block_count)
      : block_size_(block_size) {
    blocks_.reserve(block_count);
    free_.reserve(block_count);
    for (std::size_t i = 0; i < block_count; ++i) {
      blocks_.emplace_back(std::make_unique<std::byte[]>(block_size));
      free_.push_back(blocks_.back().get());
    }
  }

  std::byte* acquire() {
    if (free_.empty())
      return nullptr;
    auto* p = free_.back();
    free_.pop_back();
    return p;
  }

  void release(std::byte* p) {
    if (!p)
      return;
    free_.push_back(p);
  }

  std::size_t block_size() const { return block_size_; }
  std::size_t available() const { return free_.size(); }

private:
  std::size_t block_size_;
  std::vector<std::unique_ptr<std::byte[]>> blocks_;
  std::vector<std::byte*> free_;
};

} // namespace mdf
