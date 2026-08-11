#pragma once

#include "mdf/messages.hpp"

#include <cstdint>
#include <map>
#include <unordered_map>

namespace mdf {

struct BookLevel {
  int64_t price{};
  uint32_t qty{};
};

class OrderBook {
public:
  void apply(const Event& ev);
  void clear();
  void replace_snapshot(uint32_t symbol_id, const SnapshotMsg& snap);

  uint32_t best_bid_qty(uint32_t symbol_id) const;
  uint32_t best_ask_qty(uint32_t symbol_id) const;
  int64_t best_bid(uint32_t symbol_id) const;
  int64_t best_ask(uint32_t symbol_id) const;

  std::size_t order_count(uint32_t symbol_id) const;
  uint32_t checksum(uint32_t symbol_id) const;

  SnapshotMsg to_snapshot(uint32_t symbol_id) const;

private:
  struct Order {
    uint32_t symbol_id{};
    Side side{};
    int64_t price{};
    uint32_t qty{};
  };

  struct SymbolBook {
    std::map<int64_t, uint32_t, std::greater<int64_t>> bids;
    std::map<int64_t, uint32_t> asks;
    std::unordered_map<uint64_t, Order> orders;
  };

  SymbolBook& book(uint32_t symbol_id);
  const SymbolBook* try_book(uint32_t symbol_id) const;

  void add_qty(SymbolBook& b, Side side, int64_t price, uint32_t qty);
  void remove_qty(SymbolBook& b, Side side, int64_t price, uint32_t qty);

  std::unordered_map<uint32_t, SymbolBook> books_;
};

} // namespace mdf
