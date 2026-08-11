#pragma once

#include "mdf/messages.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace mdf {

enum class ParseError {
  Ok = 0,
  Truncated,
  BadMagic,
  BadVersion,
  BadType,
  BadLength,
  TooManyLevels,
};

struct ParseResult {
  ParseError error{ParseError::Ok};
  Event event{};
  std::size_t consumed{0};
};

ParseResult parse_message(std::span<const std::byte> data);

std::vector<std::byte> encode_add(uint64_t seq, uint32_t symbol_id, const AddMsg& m);
std::vector<std::byte> encode_modify(uint64_t seq, uint32_t symbol_id, const ModifyMsg& m);
std::vector<std::byte> encode_cancel(uint64_t seq, uint32_t symbol_id, const CancelMsg& m);
std::vector<std::byte> encode_trade(uint64_t seq, uint32_t symbol_id, const TradeMsg& m);
std::vector<std::byte> encode_snapshot(uint64_t seq, uint32_t symbol_id, const SnapshotMsg& m);
std::vector<std::byte> encode_heartbeat(uint64_t seq, uint32_t symbol_id);

std::size_t payload_size(MsgType type, std::size_t snapshot_levels = 0);

} // namespace mdf
