#include "mdf/recovery.hpp"

namespace mdf {

RecoveryManager::RecoveryManager(SnapshotProvider provider) : provider_(std::move(provider)) {}

void RecoveryManager::set_provider(SnapshotProvider provider) {
  provider_ = std::move(provider);
}

void RecoveryManager::on_gap(uint64_t gap_from, uint64_t gap_to, uint32_t symbol_id) {
  pending_ = RecoveryRequest{gap_from, gap_to, symbol_id};
}

std::optional<Event> RecoveryManager::take_snapshot() {
  if (!pending_ || !provider_)
    return std::nullopt;
  auto snap = provider_(pending_->symbol_id, pending_->gap_to + 1);
  if (snap)
    pending_.reset();
  return snap;
}

void RecoveryManager::clear() { pending_.reset(); }

Event RecoveryManager::make_snapshot_event(uint64_t seq, uint32_t symbol_id, const OrderBook& book) {
  Event ev;
  ev.type = MsgType::Snapshot;
  ev.seq = seq;
  ev.symbol_id = symbol_id;
  ev.payload = book.to_snapshot(symbol_id);
  return ev;
}

} // namespace mdf
