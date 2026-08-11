#pragma once

#include "gw/order_protocol.hpp"

#include "mdf/feed_handler.hpp"
#include "mdf/messages.hpp"
#include "mdf/recorder.hpp"

#include "OrderPool.h"
#include "Orderbook.h"

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gw {

struct GatewayStats {
  uint64_t md_applied{0};
  uint64_t orders{0};
  uint64_t trades{0};
  uint64_t rejects{0};
  uint64_t cancels{0};
};

class MatchingGateway {
public:
  MatchingGateway();

  void reset();

  void on_market_data(std::span<const std::byte> packet);
  std::vector<ExecReport> on_order(std::span<const std::byte> packet);

  const Orderbook& book() const { return *lob_; }
  Orderbook& book() { return *lob_; }
  const mdf::FeedHandler& feed() const { return feed_; }
  mdf::FeedHandler& feed() { return feed_; }
  const GatewayStats& stats() const { return stats_; }
  const std::vector<ExecReport>& trade_log() const { return trade_log_; }

  void set_memory_recorder(mdf::MemoryRecorder* rec);

private:
  void on_md_event(const mdf::Event& ev);
  void mirror_add(const mdf::AddMsg& m);
  void mirror_cancel(const mdf::CancelMsg& m);
  void mirror_modify(const mdf::ModifyMsg& m);
  void mirror_snapshot(const mdf::SnapshotMsg& m);
  void clear_lob_liquidity();

  OrderType to_order_type(TimeInForce tif) const;
  ::Side to_lob_side(Side side) const;
  ::Side to_lob_side(mdf::Side side) const;

  std::vector<ExecReport> handle_new(const OrderWireHeader& h, const ClientNewOrder& req);
  std::vector<ExecReport> handle_cancel(const OrderWireHeader& h);
  std::vector<ExecReport> handle_modify(const OrderWireHeader& h, const ModifyOrderReq& req);
  std::vector<ExecReport> trades_to_reports(uint64_t client_id, const Trades& trades);
  void forget_filled_counterparties(uint64_t client_id, const Trades& trades);

  mdf::FeedHandler feed_;
  OrderPool pool_;
  std::unique_ptr<Orderbook> lob_;
  GatewayStats stats_{};
  std::unordered_set<OrderId> md_order_ids_;
  std::unordered_map<OrderId, ::Side> md_order_sides_;
  std::unordered_set<OrderId> client_order_ids_;
  std::vector<ExecReport> trade_log_;
  mdf::MemoryRecorder* mem_recorder_{nullptr};
  uint64_t snap_synth_id_{1'000'000'000ULL};
};

} // namespace gw
