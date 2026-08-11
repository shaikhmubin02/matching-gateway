#include "mdf/seq_tracker.hpp"

namespace mdf {

SeqTracker::SeqTracker(uint64_t start_seq, std::size_t window)
    : expected_(start_seq), window_(window) {}

void SeqTracker::reset(uint64_t next_seq) {
  expected_ = next_seq;
  recovering_ = false;
  buffer_.clear();
}

void SeqTracker::end_recovery(uint64_t next_seq) {
  expected_ = next_seq;
  recovering_ = false;
  for (auto it = buffer_.begin(); it != buffer_.end();) {
    if (it->first < expected_)
      it = buffer_.erase(it);
    else
      ++it;
  }
}

void SeqTracker::flush_contiguous(std::vector<Event>& out) {
  while (true) {
    auto it = buffer_.find(expected_);
    if (it == buffer_.end())
      break;
    out.push_back(std::move(it->second));
    buffer_.erase(it);
    ++expected_;
  }
}

std::vector<Event> SeqTracker::drain_ready() {
  std::vector<Event> out;
  flush_contiguous(out);
  return out;
}

TrackResult SeqTracker::on_event(Event ev) {
  TrackResult r;
  r.expected_seq = expected_;

  if (ev.type == MsgType::Heartbeat) {
    r.status = TrackStatus::Heartbeat;
    if (ev.seq == expected_)
      ++expected_;
    return r;
  }

  if (ev.type == MsgType::Snapshot) {
    r.status = TrackStatus::Ready;
    r.ready.push_back(std::move(ev));
    return r;
  }

  if (ev.seq < expected_) {
    r.status = TrackStatus::Duplicate;
    return r;
  }

  if (ev.seq == expected_) {
    r.status = TrackStatus::Ready;
    r.ready.push_back(std::move(ev));
    ++expected_;
    flush_contiguous(r.ready);
    r.expected_seq = expected_;
    return r;
  }

  if (ev.seq > expected_ + window_) {
    r.status = TrackStatus::Gap;
    r.gap_from = expected_;
    r.gap_to = ev.seq - 1;
    buffer_[ev.seq] = std::move(ev);
    return r;
  }

  buffer_[ev.seq] = std::move(ev);
  r.status = TrackStatus::Buffered;
  return r;
}

} // namespace mdf
