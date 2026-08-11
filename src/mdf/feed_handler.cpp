#include "mdf/feed_handler.hpp"

#include "mdf/protocol.hpp"

namespace mdf {

FeedHandler::FeedHandler(std::size_t queue_capacity, std::size_t ooo_window)
    : tracker_(1, ooo_window), queue_(queue_capacity) {}

void FeedHandler::set_snapshot_provider(RecoveryManager::SnapshotProvider provider) {
  recovery_.set_provider(std::move(provider));
}

void FeedHandler::reset(uint64_t next_seq) {
  stop_consumer();
  tracker_.reset(next_seq);
  book_.clear();
  recovery_.clear();
  stats_ = {};
  Event junk;
  while (queue_.try_pop(junk)) {
  }
}

void FeedHandler::dispatch_ready(std::vector<Event>&& ready) {
  for (auto& ev : ready) {
    while (!queue_.try_push(std::move(ev))) {
      drain_to_book(256);
    }
  }
}

void FeedHandler::try_recover(uint32_t symbol_id) {
  (void)symbol_id;
  auto snap = recovery_.take_snapshot();
  if (!snap)
    return;

  book_.apply(*snap);
  if (listener_)
    listener_(*snap);
  ++stats_.recovered;
  tracker_.end_recovery(snap->seq + 1);
  dispatch_ready(tracker_.drain_ready());
}

bool FeedHandler::ingest(std::span<const std::byte> packet) {
  auto parsed = parse_message(packet);
  if (parsed.error != ParseError::Ok) {
    ++stats_.malformed;
    return false;
  }
  ++stats_.parsed;

  if (recorder_)
    recorder_->write_packet(parsed.event.seq, packet.subspan(0, parsed.consumed));
  if (mem_recorder_)
    mem_recorder_->write_packet(parsed.event.seq, packet.subspan(0, parsed.consumed));

  const uint32_t symbol_id = parsed.event.symbol_id;

  if (parsed.event.type == MsgType::Snapshot) {
    book_.apply(parsed.event);
    if (listener_)
      listener_(parsed.event);
    tracker_.end_recovery(parsed.event.seq + 1);
    ++stats_.applied;
    ++stats_.recovered;
    auto buffered = tracker_.drain_ready();
    dispatch_ready(std::move(buffered));
    return true;
  }

  auto tr = tracker_.on_event(std::move(parsed.event));
  switch (tr.status) {
  case TrackStatus::Duplicate:
    ++stats_.duplicates;
    return false;
  case TrackStatus::Heartbeat:
    return false;
  case TrackStatus::Buffered:
    return false;
  case TrackStatus::Gap:
    ++stats_.gaps;
    tracker_.begin_recovery();
    recovery_.on_gap(tr.gap_from, tr.gap_to, symbol_id);
    try_recover(symbol_id);
    return true;
  case TrackStatus::Ready:
    dispatch_ready(std::move(tr.ready));
    return true;
  }
  return false;
}

void FeedHandler::process(std::span<const std::byte> packet) {
  ingest(packet);
  drain_to_book();
}

std::size_t FeedHandler::drain_to_book(std::size_t max_events) {
  std::size_t n = 0;
  Event ev;
  while (n < max_events && queue_.try_pop(ev)) {
    book_.apply(ev);
    if (listener_)
      listener_(ev);
    ++stats_.applied;
    ++n;
  }
  return n;
}

void FeedHandler::consumer_loop() {
  Event ev;
  while (running_.load(std::memory_order_acquire)) {
    if (queue_.try_pop(ev)) {
      book_.apply(ev);
      if (listener_)
        listener_(ev);
      ++stats_.applied;
    } else {
      std::this_thread::yield();
    }
  }
  while (queue_.try_pop(ev)) {
    book_.apply(ev);
    if (listener_)
      listener_(ev);
    ++stats_.applied;
  }
}

void FeedHandler::start_consumer() {
  if (running_.exchange(true))
    return;
  consumer_ = std::thread([this] { consumer_loop(); });
}

void FeedHandler::stop_consumer() {
  if (!running_.exchange(false))
    return;
  if (consumer_.joinable())
    consumer_.join();
}

} // namespace mdf
