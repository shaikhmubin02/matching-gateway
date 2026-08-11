#include "gw/order_protocol.hpp"

#include <cstring>

namespace gw {
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

} // namespace

OrderParseResult parse_order_message(std::span<const std::byte> data) {
  OrderParseResult r;
  std::size_t off = 0;
  if (!read_pod(data, off, r.header)) {
    r.error = OrderParseError::Truncated;
    return r;
  }
  if (r.header.magic != kOrderMagic) {
    r.error = OrderParseError::BadMagic;
    return r;
  }
  if (r.header.version != kOrderVersion) {
    r.error = OrderParseError::BadVersion;
    return r;
  }

  const auto type = static_cast<OrderMsgType>(r.header.type);
  switch (type) {
  case OrderMsgType::New: {
    uint8_t side{}, tif{};
    if (!read_pod(data, off, side) || !read_pod(data, off, tif) ||
        !read_pod(data, off, r.new_order.price) || !read_pod(data, off, r.new_order.qty)) {
      r.error = OrderParseError::Truncated;
      return r;
    }
    r.new_order.side = static_cast<Side>(side);
    r.new_order.tif = static_cast<TimeInForce>(tif);
    break;
  }
  case OrderMsgType::Cancel:
    break;
  case OrderMsgType::Modify: {
    uint8_t side{};
    if (!read_pod(data, off, side) || !read_pod(data, off, r.modify.price) ||
        !read_pod(data, off, r.modify.qty)) {
      r.error = OrderParseError::Truncated;
      return r;
    }
    r.modify.side = static_cast<Side>(side);
    break;
  }
  default:
    r.error = OrderParseError::BadType;
    return r;
  }

  r.consumed = off;
  r.error = OrderParseError::Ok;
  return r;
}

std::vector<std::byte> encode_new_order(uint64_t client_order_id, uint32_t symbol_id,
                                        const ClientNewOrder& m) {
  std::vector<std::byte> out;
  OrderWireHeader h{kOrderMagic, kOrderVersion, static_cast<uint8_t>(OrderMsgType::New),
                    client_order_id, symbol_id};
  append_pod(out, h);
  append_pod(out, static_cast<uint8_t>(m.side));
  append_pod(out, static_cast<uint8_t>(m.tif));
  append_pod(out, m.price);
  append_pod(out, m.qty);
  return out;
}

std::vector<std::byte> encode_cancel_order(uint64_t client_order_id, uint32_t symbol_id) {
  std::vector<std::byte> out;
  OrderWireHeader h{kOrderMagic, kOrderVersion, static_cast<uint8_t>(OrderMsgType::Cancel),
                    client_order_id, symbol_id};
  append_pod(out, h);
  return out;
}

std::vector<std::byte> encode_modify_order(uint64_t client_order_id, uint32_t symbol_id,
                                           const ModifyOrderReq& m) {
  std::vector<std::byte> out;
  OrderWireHeader h{kOrderMagic, kOrderVersion, static_cast<uint8_t>(OrderMsgType::Modify),
                    client_order_id, symbol_id};
  append_pod(out, h);
  append_pod(out, static_cast<uint8_t>(m.side));
  append_pod(out, m.price);
  append_pod(out, m.qty);
  return out;
}

} // namespace gw
