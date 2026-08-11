#include "mdf/protocol.hpp"

#include <cstring>

namespace mdf {
namespace {

template <typename T>
bool read_pod(std::span<const std::byte> data, std::size_t& off, T& out) {
  if (off + sizeof(T) > data.size())
    return false;
  std::memcpy(&out, data.data() + off, sizeof(T));
  off += sizeof(T);
  return true;
}

template <typename T>
void append_pod(std::vector<std::byte>& out, const T& v) {
  const auto* p = reinterpret_cast<const std::byte*>(&v);
  out.insert(out.end(), p, p + sizeof(T));
}

void write_header(std::vector<std::byte>& out, MsgType type, uint64_t seq, uint32_t symbol_id) {
  WireHeader h{};
  h.magic = kMagic;
  h.version = kVersion;
  h.type = static_cast<uint8_t>(type);
  h.seq = seq;
  h.symbol_id = symbol_id;
  append_pod(out, h);
}

} // namespace

std::size_t payload_size(MsgType type, std::size_t snapshot_levels) {
  switch (type) {
  case MsgType::Add:
    return sizeof(uint64_t) + 1 + sizeof(int64_t) + sizeof(uint32_t);
  case MsgType::Modify:
    return sizeof(uint64_t) + sizeof(int64_t) + sizeof(uint32_t);
  case MsgType::Cancel:
    return sizeof(uint64_t);
  case MsgType::Trade:
    return sizeof(uint64_t) + sizeof(int64_t) + sizeof(uint32_t) + 1;
  case MsgType::Snapshot:
    return sizeof(uint32_t) * 2 + snapshot_levels * (1 + sizeof(int64_t) + sizeof(uint32_t));
  case MsgType::Heartbeat:
    return 0;
  }
  return 0;
}

ParseResult parse_message(std::span<const std::byte> data) {
  ParseResult r;
  if (data.size() < kHeaderSize) {
    r.error = ParseError::Truncated;
    return r;
  }

  WireHeader h{};
  std::memcpy(&h, data.data(), sizeof(h));
  if (h.magic != kMagic) {
    r.error = ParseError::BadMagic;
    return r;
  }
  if (h.version != kVersion) {
    r.error = ParseError::BadVersion;
    return r;
  }

  const auto type = static_cast<MsgType>(h.type);
  std::size_t off = kHeaderSize;
  r.event.seq = h.seq;
  r.event.symbol_id = h.symbol_id;
  r.event.type = type;

  switch (type) {
  case MsgType::Add: {
    AddMsg m{};
    uint8_t side = 0;
    if (!read_pod(data, off, m.order_id) || !read_pod(data, off, side) ||
        !read_pod(data, off, m.price) || !read_pod(data, off, m.qty)) {
      r.error = ParseError::Truncated;
      return r;
    }
    if (side > 1) {
      r.error = ParseError::BadType;
      return r;
    }
    m.side = static_cast<Side>(side);
    r.event.payload = m;
    break;
  }
  case MsgType::Modify: {
    ModifyMsg m{};
    if (!read_pod(data, off, m.order_id) || !read_pod(data, off, m.price) ||
        !read_pod(data, off, m.qty)) {
      r.error = ParseError::Truncated;
      return r;
    }
    r.event.payload = m;
    break;
  }
  case MsgType::Cancel: {
    CancelMsg m{};
    if (!read_pod(data, off, m.order_id)) {
      r.error = ParseError::Truncated;
      return r;
    }
    r.event.payload = m;
    break;
  }
  case MsgType::Trade: {
    TradeMsg m{};
    uint8_t side = 0;
    if (!read_pod(data, off, m.trade_id) || !read_pod(data, off, m.price) ||
        !read_pod(data, off, m.qty) || !read_pod(data, off, side)) {
      r.error = ParseError::Truncated;
      return r;
    }
    if (side > 1) {
      r.error = ParseError::BadType;
      return r;
    }
    m.aggressor = static_cast<Side>(side);
    r.event.payload = m;
    break;
  }
  case MsgType::Snapshot: {
    SnapshotMsg m{};
    if (!read_pod(data, off, m.level_count) || !read_pod(data, off, m.checksum)) {
      r.error = ParseError::Truncated;
      return r;
    }
    if (m.level_count > kMaxSnapshotLevels) {
      r.error = ParseError::TooManyLevels;
      return r;
    }
    m.levels.resize(m.level_count);
    for (uint32_t i = 0; i < m.level_count; ++i) {
      uint8_t side = 0;
      if (!read_pod(data, off, side) || !read_pod(data, off, m.levels[i].price) ||
          !read_pod(data, off, m.levels[i].qty)) {
        r.error = ParseError::Truncated;
        return r;
      }
      if (side > 1) {
        r.error = ParseError::BadType;
        return r;
      }
      m.levels[i].side = static_cast<Side>(side);
    }
    r.event.payload = std::move(m);
    break;
  }
  case MsgType::Heartbeat:
    r.event.payload = HeartbeatMsg{};
    break;
  default:
    r.error = ParseError::BadType;
    return r;
  }

  r.consumed = off;
  r.error = ParseError::Ok;
  return r;
}

std::vector<std::byte> encode_add(uint64_t seq, uint32_t symbol_id, const AddMsg& m) {
  std::vector<std::byte> out;
  out.reserve(kHeaderSize + payload_size(MsgType::Add));
  write_header(out, MsgType::Add, seq, symbol_id);
  append_pod(out, m.order_id);
  append_pod(out, static_cast<uint8_t>(m.side));
  append_pod(out, m.price);
  append_pod(out, m.qty);
  return out;
}

std::vector<std::byte> encode_modify(uint64_t seq, uint32_t symbol_id, const ModifyMsg& m) {
  std::vector<std::byte> out;
  out.reserve(kHeaderSize + payload_size(MsgType::Modify));
  write_header(out, MsgType::Modify, seq, symbol_id);
  append_pod(out, m.order_id);
  append_pod(out, m.price);
  append_pod(out, m.qty);
  return out;
}

std::vector<std::byte> encode_cancel(uint64_t seq, uint32_t symbol_id, const CancelMsg& m) {
  std::vector<std::byte> out;
  out.reserve(kHeaderSize + payload_size(MsgType::Cancel));
  write_header(out, MsgType::Cancel, seq, symbol_id);
  append_pod(out, m.order_id);
  return out;
}

std::vector<std::byte> encode_trade(uint64_t seq, uint32_t symbol_id, const TradeMsg& m) {
  std::vector<std::byte> out;
  out.reserve(kHeaderSize + payload_size(MsgType::Trade));
  write_header(out, MsgType::Trade, seq, symbol_id);
  append_pod(out, m.trade_id);
  append_pod(out, m.price);
  append_pod(out, m.qty);
  append_pod(out, static_cast<uint8_t>(m.aggressor));
  return out;
}

std::vector<std::byte> encode_snapshot(uint64_t seq, uint32_t symbol_id, const SnapshotMsg& m) {
  std::vector<std::byte> out;
  out.reserve(kHeaderSize + payload_size(MsgType::Snapshot, m.levels.size()));
  write_header(out, MsgType::Snapshot, seq, symbol_id);
  const uint32_t count = static_cast<uint32_t>(m.levels.size());
  append_pod(out, count);
  append_pod(out, m.checksum);
  for (const auto& lvl : m.levels) {
    append_pod(out, static_cast<uint8_t>(lvl.side));
    append_pod(out, lvl.price);
    append_pod(out, lvl.qty);
  }
  return out;
}

std::vector<std::byte> encode_heartbeat(uint64_t seq, uint32_t symbol_id) {
  std::vector<std::byte> out;
  out.reserve(kHeaderSize);
  write_header(out, MsgType::Heartbeat, seq, symbol_id);
  return out;
}

} // namespace mdf
