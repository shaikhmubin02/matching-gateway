#pragma once

#include "mdf/messages.hpp"
#include "mdf/order_book.hpp"
#include "mdf/seq_tracker.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace mdf {

struct RecoveryRequest {
  uint64_t gap_from{};
  uint64_t gap_to{};
  uint32_t symbol_id{};
};

class RecoveryManager {
public:
  using SnapshotProvider = std::function<std::optional<Event>(uint32_t symbol_id, uint64_t seq)>;

  explicit RecoveryManager(SnapshotProvider provider = {});

  void set_provider(SnapshotProvider provider);
  bool needs_recovery() const { return pending_.has_value(); }
  const std::optional<RecoveryRequest>& pending() const { return pending_; }

  void on_gap(uint64_t gap_from, uint64_t gap_to, uint32_t symbol_id);
  std::optional<Event> take_snapshot();
  void clear();

  // Build a recovery snapshot event from a live book (for demos / tests).
  static Event make_snapshot_event(uint64_t seq, uint32_t symbol_id, const OrderBook& book);

private:
  SnapshotProvider provider_;
  std::optional<RecoveryRequest> pending_;
};

} // namespace mdf
