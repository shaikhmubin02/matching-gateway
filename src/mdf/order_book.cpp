#include "mdf/order_book.hpp"

namespace mdf {

OrderBook::SymbolBook& OrderBook::book(uint32_t symbol_id) {
  return books_[symbol_id];
}

const OrderBook::SymbolBook* OrderBook::try_book(uint32_t symbol_id) const {
  auto it = books_.find(symbol_id);
  if (it == books_.end())
    return nullptr;
  return &it->second;
}

void OrderBook::add_qty(SymbolBook& b, Side side, int64_t price, uint32_t qty) {
  if (side == Side::Bid)
    b.bids[price] += qty;
  else
    b.asks[price] += qty;
}

void OrderBook::remove_qty(SymbolBook& b, Side side, int64_t price, uint32_t qty) {
  auto erase_empty = [](auto& levels, int64_t px) {
    auto it = levels.find(px);
    if (it == levels.end())
      return;
    if (it->second == 0)
      levels.erase(it);
  };

  if (side == Side::Bid) {
    auto it = b.bids.find(price);
    if (it == b.bids.end())
      return;
    if (it->second <= qty)
      b.bids.erase(it);
    else
      it->second -= qty;
    erase_empty(b.bids, price);
  } else {
    auto it = b.asks.find(price);
    if (it == b.asks.end())
      return;
    if (it->second <= qty)
      b.asks.erase(it);
    else
      it->second -= qty;
    erase_empty(b.asks, price);
  }
}

void OrderBook::clear() { books_.clear(); }

void OrderBook::replace_snapshot(uint32_t symbol_id, const SnapshotMsg& snap) {
  SymbolBook b;
  for (const auto& lvl : snap.levels) {
    if (lvl.side == Side::Bid)
      b.bids[lvl.price] += lvl.qty;
    else
      b.asks[lvl.price] += lvl.qty;
  }
  books_[symbol_id] = std::move(b);
}

void OrderBook::apply(const Event& ev) {
  if (ev.type == MsgType::Heartbeat)
    return;

  if (ev.type == MsgType::Snapshot) {
    replace_snapshot(ev.symbol_id, std::get<SnapshotMsg>(ev.payload));
    return;
  }

  auto& b = book(ev.symbol_id);

  switch (ev.type) {
  case MsgType::Add: {
    const auto& m = std::get<AddMsg>(ev.payload);
    if (b.orders.contains(m.order_id))
      return;
    b.orders[m.order_id] = Order{ev.symbol_id, m.side, m.price, m.qty};
    add_qty(b, m.side, m.price, m.qty);
    break;
  }
  case MsgType::Modify: {
    const auto& m = std::get<ModifyMsg>(ev.payload);
    auto it = b.orders.find(m.order_id);
    if (it == b.orders.end())
      return;
    remove_qty(b, it->second.side, it->second.price, it->second.qty);
    it->second.price = m.price;
    it->second.qty = m.qty;
    add_qty(b, it->second.side, m.price, m.qty);
    break;
  }
  case MsgType::Cancel: {
    const auto& m = std::get<CancelMsg>(ev.payload);
    auto it = b.orders.find(m.order_id);
    if (it == b.orders.end())
      return;
    remove_qty(b, it->second.side, it->second.price, it->second.qty);
    b.orders.erase(it);
    break;
  }
  case MsgType::Trade: {
    const auto& m = std::get<TradeMsg>(ev.payload);
    // Trades may reduce resting liquidity on the passive side.
    const Side passive = (m.aggressor == Side::Bid) ? Side::Ask : Side::Bid;
    remove_qty(b, passive, m.price, m.qty);
    break;
  }
  default:
    break;
  }
}

int64_t OrderBook::best_bid(uint32_t symbol_id) const {
  const auto* b = try_book(symbol_id);
  if (!b || b->bids.empty())
    return 0;
  return b->bids.begin()->first;
}

int64_t OrderBook::best_ask(uint32_t symbol_id) const {
  const auto* b = try_book(symbol_id);
  if (!b || b->asks.empty())
    return 0;
  return b->asks.begin()->first;
}

uint32_t OrderBook::best_bid_qty(uint32_t symbol_id) const {
  const auto* b = try_book(symbol_id);
  if (!b || b->bids.empty())
    return 0;
  return b->bids.begin()->second;
}

uint32_t OrderBook::best_ask_qty(uint32_t symbol_id) const {
  const auto* b = try_book(symbol_id);
  if (!b || b->asks.empty())
    return 0;
  return b->asks.begin()->second;
}

std::size_t OrderBook::order_count(uint32_t symbol_id) const {
  const auto* b = try_book(symbol_id);
  return b ? b->orders.size() : 0;
}

uint32_t OrderBook::checksum(uint32_t symbol_id) const {
  const auto* b = try_book(symbol_id);
  if (!b)
    return 0;
  uint32_t c = 0;
  for (const auto& [px, qty] : b->bids)
    c = c * 131u + static_cast<uint32_t>(px) + qty * 17u;
  for (const auto& [px, qty] : b->asks)
    c = c * 131u + static_cast<uint32_t>(px) + qty * 19u;
  return c;
}

SnapshotMsg OrderBook::to_snapshot(uint32_t symbol_id) const {
  SnapshotMsg snap;
  const auto* b = try_book(symbol_id);
  if (!b) {
    snap.checksum = 0;
    return snap;
  }
  for (const auto& [px, qty] : b->bids)
    snap.levels.push_back(SnapshotLevel{Side::Bid, px, qty});
  for (const auto& [px, qty] : b->asks)
    snap.levels.push_back(SnapshotLevel{Side::Ask, px, qty});
  snap.level_count = static_cast<uint32_t>(snap.levels.size());
  snap.checksum = checksum(symbol_id);
  return snap;
}

} // namespace mdf
