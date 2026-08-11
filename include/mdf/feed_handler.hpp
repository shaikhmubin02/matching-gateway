#pragma once

#include "mdf/messages.hpp"
#include "mdf/order_book.hpp"
#include "mdf/pool.hpp"
#include "mdf/queue.hpp"
#include "mdf/recorder.hpp"
#include "mdf/recovery.hpp"
#include "mdf/seq_tracker.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace mdf {

struct FeedStats {
  uint64_t parsed{0};
  uint64_t applied{0};
  uint64_t duplicates{0};
  uint64_t gaps{0};
  uint64_t malformed{0};
  uint64_t recovered{0};
};

class FeedHandler {
public:
  using EventListener = std::function<void(const Event&)>;

  FeedHandler(std::size_t queue_capacity = 1 << 16, std::size_t ooo_window = 64);

  void set_recorder(Recorder* rec) { recorder_ = rec; }
  void set_memory_recorder(MemoryRecorder* rec) { mem_recorder_ = rec; }
  void set_snapshot_provider(RecoveryManager::SnapshotProvider provider);
  void set_event_listener(EventListener listener) { listener_ = std::move(listener); }

  // Parse + sequence; returns true if at least one event was enqueued.
  bool ingest(std::span<const std::byte> packet);

  // Single-threaded path: ingest then drain into the book.
  void process(std::span<const std::byte> packet);

  void start_consumer();
  void stop_consumer();

  std::size_t drain_to_book(std::size_t max_events = SIZE_MAX);
  OrderBook& book() { return book_; }
  const OrderBook& book() const { return book_; }
  SeqTracker& tracker() { return tracker_; }
  const FeedStats& stats() const { return stats_; }
  FeedStats& stats() { return stats_; }

  void reset(uint64_t next_seq = 1);

private:
  void consumer_loop();
  void dispatch_ready(std::vector<Event>&& ready);
  void try_recover(uint32_t symbol_id);

  OrderBook book_;
  SeqTracker tracker_;
  RecoveryManager recovery_;
  BoundedQueue<Event> queue_;
  Recorder* recorder_{nullptr};
  MemoryRecorder* mem_recorder_{nullptr};
  EventListener listener_{};
  FeedStats stats_{};
  std::atomic<bool> running_{false};
  std::thread consumer_;
};

} // namespace mdf
