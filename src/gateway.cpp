#include "gw/gateway.hpp"

#include "mdf/protocol.hpp"

#include <vector>

namespace gw {

MatchingGateway::MatchingGateway()
    : pool_(1 << 16)
    , lob_(std::make_unique<Orderbook>(
          OrderbookConfig{.enableGoodForDayPrune = false, .enableLocking = false})) {
  feed_.set_event_listener([this](const mdf::Event& ev) { on_md_event(ev); });
}

void MatchingGateway::clear_lob_liquidity() {
  for (OrderId id : std::vector<OrderId>(md_order_ids_.begin(), md_order_ids_.end()))
    lob_->CancelOrder(id);
  for (OrderId id : std::vector<OrderId>(client_order_ids_.begin(), client_order_ids_.end()))
    lob_->CancelOrder(id);
  md_order_ids_.clear();
  md_order_sides_.clear();
  client_order_ids_.clear();
}

void MatchingGateway::reset() {
  feed_.reset(1);
  clear_lob_liquidity();
  lob_ = std::make_unique<Orderbook>(
      OrderbookConfig{.enableGoodForDayPrune = false, .enableLocking = false});
  trade_log_.clear();
  stats_ = {};
  snap_synth_id_ = 1'000'000'000ULL;
}

void MatchingGateway::set_memory_recorder(mdf::MemoryRecorder* rec) {
  mem_recorder_ = rec;
  feed_.set_memory_recorder(rec);
}

OrderType MatchingGateway::to_order_type(TimeInForce tif) const {
  switch (tif) {
  case TimeInForce::Gtc:
    return OrderType::GoodTillCancel;
  case TimeInForce::Fak:
    return OrderType::FillAndKill;
  case TimeInForce::Fok:
    return OrderType::FillOrKill;
  case TimeInForce::Gfd:
    return OrderType::GoodForDay;
  case TimeInForce::Market:
    return OrderType::Market;
  }
  return OrderType::GoodTillCancel;
}

::Side MatchingGateway::to_lob_side(Side side) const {
  return side == Side::Buy ? ::Side::Buy : ::Side::Sell;
}

::Side MatchingGateway::to_lob_side(mdf::Side side) const {
  return side == mdf::Side::Bid ? ::Side::Buy : ::Side::Sell;
}

void MatchingGateway::on_market_data(std::span<const std::byte> packet) {
  feed_.process(packet);
}

void MatchingGateway::on_md_event(const mdf::Event& ev) {
  ++stats_.md_applied;
  switch (ev.type) {
  case mdf::MsgType::Add:
    mirror_add(std::get<mdf::AddMsg>(ev.payload));
    break;
  case mdf::MsgType::Cancel:
    mirror_cancel(std::get<mdf::CancelMsg>(ev.payload));
    break;
  case mdf::MsgType::Modify:
    mirror_modify(std::get<mdf::ModifyMsg>(ev.payload));
    break;
  case mdf::MsgType::Snapshot:
    mirror_snapshot(std::get<mdf::SnapshotMsg>(ev.payload));
    break;
  case mdf::MsgType::Trade:
  case mdf::MsgType::Heartbeat:
    break;
  }
}

void MatchingGateway::mirror_add(const mdf::AddMsg& m) {
  if (md_order_ids_.contains(m.order_id) || client_order_ids_.contains(m.order_id))
    return;
  const ::Side side = to_lob_side(m.side);
  auto order = pool_.Acquire(OrderType::GoodTillCancel, m.order_id, side,
                             static_cast<Price>(m.price), m.qty);
  lob_->AddOrder(order);
  md_order_ids_.insert(m.order_id);
  md_order_sides_[m.order_id] = side;
}

void MatchingGateway::mirror_cancel(const mdf::CancelMsg& m) {
  if (!md_order_ids_.contains(m.order_id))
    return;
  lob_->CancelOrder(m.order_id);
  md_order_ids_.erase(m.order_id);
  md_order_sides_.erase(m.order_id);
}

void MatchingGateway::mirror_modify(const mdf::ModifyMsg& m) {
  auto it = md_order_sides_.find(m.order_id);
  if (it == md_order_sides_.end())
    return;
  lob_->ModifyOrder(OrderModify{m.order_id, it->second, static_cast<Price>(m.price), m.qty});
}

void MatchingGateway::mirror_snapshot(const mdf::SnapshotMsg& snap) {
  for (OrderId id : std::vector<OrderId>(md_order_ids_.begin(), md_order_ids_.end()))
    lob_->CancelOrder(id);
  md_order_ids_.clear();
  md_order_sides_.clear();

  for (const auto& lvl : snap.levels) {
    const OrderId id = ++snap_synth_id_;
    const ::Side side = to_lob_side(lvl.side);
    auto order = pool_.Acquire(OrderType::GoodTillCancel, id, side,
                               static_cast<Price>(lvl.price), lvl.qty);
    lob_->AddOrder(order);
    md_order_ids_.insert(id);
    md_order_sides_[id] = side;
  }
}

void MatchingGateway::forget_filled_counterparties(uint64_t client_id, const Trades& trades) {
  for (const auto& t : trades) {
    for (OrderId id : {t.GetBidTrade().orderId_, t.GetAskTrade().orderId_}) {
      if (id == client_id)
        continue;
      md_order_ids_.erase(id);
      md_order_sides_.erase(id);
    }
  }
}

std::vector<ExecReport> MatchingGateway::trades_to_reports(uint64_t client_id,
                                                          const Trades& trades) {
  std::vector<ExecReport> out;
  out.reserve(trades.size());
  for (const auto& t : trades) {
    const auto& bid = t.GetBidTrade();
    const auto& ask = t.GetAskTrade();
    ExecReport er;
    er.type = ExecType::Trade;
    er.client_order_id = client_id;
    if (bid.orderId_ == client_id) {
      er.other_order_id = ask.orderId_;
      er.price = bid.price_;
      er.qty = bid.quantity_;
    } else {
      er.other_order_id = bid.orderId_;
      er.price = ask.price_;
      er.qty = ask.quantity_;
    }
    out.push_back(er);
    trade_log_.push_back(er);
    ++stats_.trades;
  }
  forget_filled_counterparties(client_id, trades);
  return out;
}

std::vector<ExecReport> MatchingGateway::handle_new(const OrderWireHeader& h,
                                                   const ClientNewOrder& req) {
  std::vector<ExecReport> reports;
  ++stats_.orders;

  if (client_order_ids_.contains(h.client_order_id) || md_order_ids_.contains(h.client_order_id)) {
    ++stats_.rejects;
    reports.push_back(ExecReport{ExecType::Reject, h.client_order_id, 0, 0, 0,
                                 RejectReason::DuplicateId});
    return reports;
  }

  const auto before = lob_->Size();
  auto order = pool_.Acquire(to_order_type(req.tif), h.client_order_id, to_lob_side(req.side),
                             req.price, req.qty);
  auto trades = lob_->AddOrder(order);
  const auto after = lob_->Size();

  if (req.tif == TimeInForce::Fok && trades.empty() && after == before) {
    ++stats_.rejects;
    reports.push_back(ExecReport{ExecType::Reject, h.client_order_id, 0, 0, 0,
                                 RejectReason::FokUnfilled});
    return reports;
  }

  if (req.tif == TimeInForce::Market && trades.empty() && after == before) {
    ++stats_.rejects;
    reports.push_back(ExecReport{ExecType::Reject, h.client_order_id, 0, 0, 0,
                                 RejectReason::MarketEmpty});
    return reports;
  }

  // Prevent duplicate client ids (resting or completed).
  client_order_ids_.insert(h.client_order_id);

  reports.push_back(ExecReport{ExecType::Ack, h.client_order_id, 0, req.price, req.qty,
                               RejectReason::None});
  auto tr = trades_to_reports(h.client_order_id, trades);
  reports.insert(reports.end(), tr.begin(), tr.end());
  return reports;
}

std::vector<ExecReport> MatchingGateway::handle_cancel(const OrderWireHeader& h) {
  std::vector<ExecReport> reports;
  ++stats_.orders;
  if (!client_order_ids_.contains(h.client_order_id)) {
    ++stats_.rejects;
    reports.push_back(ExecReport{ExecType::Reject, h.client_order_id, 0, 0, 0,
                                 RejectReason::NotFound});
    return reports;
  }
  const auto before = lob_->Size();
  lob_->CancelOrder(h.client_order_id);
  const auto after = lob_->Size();
  client_order_ids_.erase(h.client_order_id);
  if (after == before) {
    ++stats_.rejects;
    reports.push_back(ExecReport{ExecType::Reject, h.client_order_id, 0, 0, 0,
                                 RejectReason::NotFound});
    return reports;
  }
  ++stats_.cancels;
  reports.push_back(ExecReport{ExecType::CancelAck, h.client_order_id, 0, 0, 0,
                               RejectReason::None});
  return reports;
}

std::vector<ExecReport> MatchingGateway::handle_modify(const OrderWireHeader& h,
                                                      const ModifyOrderReq& req) {
  std::vector<ExecReport> reports;
  ++stats_.orders;
  if (!client_order_ids_.contains(h.client_order_id)) {
    ++stats_.rejects;
    reports.push_back(ExecReport{ExecType::Reject, h.client_order_id, 0, 0, 0,
                                 RejectReason::NotFound});
    return reports;
  }

  auto trades =
      lob_->ModifyOrder(OrderModify{h.client_order_id, to_lob_side(req.side), req.price, req.qty});
  reports.push_back(ExecReport{ExecType::Ack, h.client_order_id, 0, req.price, req.qty,
                               RejectReason::None});
  auto tr = trades_to_reports(h.client_order_id, trades);
  reports.insert(reports.end(), tr.begin(), tr.end());
  return reports;
}

std::vector<ExecReport> MatchingGateway::on_order(std::span<const std::byte> packet) {
  (void)mem_recorder_;
  auto parsed = parse_order_message(packet);
  if (parsed.error != OrderParseError::Ok) {
    ++stats_.rejects;
    return {ExecReport{ExecType::Reject, 0, 0, 0, 0, RejectReason::BadMessage}};
  }

  switch (static_cast<OrderMsgType>(parsed.header.type)) {
  case OrderMsgType::New:
    return handle_new(parsed.header, parsed.new_order);
  case OrderMsgType::Cancel:
    return handle_cancel(parsed.header);
  case OrderMsgType::Modify:
    return handle_modify(parsed.header, parsed.modify);
  }
  ++stats_.rejects;
  return {ExecReport{ExecType::Reject, parsed.header.client_order_id, 0, 0, 0,
                     RejectReason::BadMessage}};
}

} // namespace gw
