#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

namespace mdf {

template <typename T>
class BoundedQueue {
public:
  explicit BoundedQueue(std::size_t capacity)
      : capacity_(next_pow2(capacity)), mask_(capacity_ - 1), slots_(capacity_) {
    for (std::size_t i = 0; i < capacity_; ++i)
      slots_[i].seq.store(i, std::memory_order_relaxed);
  }

  bool try_push(const T& item) {
    const uint64_t pos = write_pos_.load(std::memory_order_relaxed);
    Slot& slot = slots_[pos & mask_];
    const uint64_t seq = slot.seq.load(std::memory_order_acquire);
    const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
    if (dif != 0)
      return false;
    slot.value = item;
    slot.seq.store(pos + 1, std::memory_order_release);
    write_pos_.store(pos + 1, std::memory_order_relaxed);
    return true;
  }

  bool try_push(T&& item) {
    const uint64_t pos = write_pos_.load(std::memory_order_relaxed);
    Slot& slot = slots_[pos & mask_];
    const uint64_t seq = slot.seq.load(std::memory_order_acquire);
    const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
    if (dif != 0)
      return false;
    slot.value = std::move(item);
    slot.seq.store(pos + 1, std::memory_order_release);
    write_pos_.store(pos + 1, std::memory_order_relaxed);
    return true;
  }

  bool push(T item) {
    while (!try_push(std::move(item)))
      std::this_thread::yield();
    return true;
  }

  bool try_pop(T& out) {
    const uint64_t pos = read_pos_.load(std::memory_order_relaxed);
    Slot& slot = slots_[pos & mask_];
    const uint64_t seq = slot.seq.load(std::memory_order_acquire);
    const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
    if (dif != 0)
      return false;
    out = std::move(slot.value);
    slot.seq.store(pos + capacity_, std::memory_order_release);
    read_pos_.store(pos + 1, std::memory_order_relaxed);
    return true;
  }

  bool pop(T& out) {
    while (!try_pop(out))
      std::this_thread::yield();
    return true;
  }

  std::size_t capacity() const { return capacity_; }

private:
  struct Slot {
    std::atomic<uint64_t> seq{0};
    T value{};
  };

  static std::size_t next_pow2(std::size_t n) {
    std::size_t p = 1;
    while (p < n)
      p <<= 1;
    return p;
  }

  const std::size_t capacity_;
  const std::size_t mask_;
  std::vector<Slot> slots_;
  alignas(64) std::atomic<uint64_t> write_pos_{0};
  alignas(64) std::atomic<uint64_t> read_pos_{0};
};

} // namespace mdf
