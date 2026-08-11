#pragma once

#include "mdf/messages.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mdf {

enum class TrackStatus {
  Ready,
  Duplicate,
  Buffered,
  Gap,
  Heartbeat,
};

struct TrackResult {
  TrackStatus status{TrackStatus::Ready};
  std::vector<Event> ready;
  uint64_t expected_seq{0};
  uint64_t gap_from{0};
  uint64_t gap_to{0};
};

class SeqTracker {
public:
  explicit SeqTracker(uint64_t start_seq = 1, std::size_t window = 64);

  TrackResult on_event(Event ev);
  void reset(uint64_t next_seq);
  uint64_t expected() const { return expected_; }
  bool recovering() const { return recovering_; }
  void begin_recovery() { recovering_ = true; }
  void end_recovery(uint64_t next_seq);

  std::vector<Event> drain_ready();

private:
  void flush_contiguous(std::vector<Event>& out);

  uint64_t expected_;
  std::size_t window_;
  bool recovering_{false};
  std::unordered_map<uint64_t, Event> buffer_;
};

} // namespace mdf
