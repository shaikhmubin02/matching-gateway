#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gw {

enum class OrderMsgType : uint8_t {
  New = 1,
  Cancel = 2,
  Modify = 3,
};

enum class TimeInForce : uint8_t {
  Gtc = 1,
  Fak = 2,
  Fok = 3,
  Gfd = 4,
  Market = 5,
};

enum class Side : uint8_t {
  Buy = 0,
  Sell = 1,
};

enum class ExecType : uint8_t {
  Ack = 1,
  Reject = 2,
  Trade = 3,
  CancelAck = 4,
};

enum class RejectReason : uint8_t {
  None = 0,
  DuplicateId = 1,
  NotFound = 2,
  FokUnfilled = 3,
  BadMessage = 4,
  MarketEmpty = 5,
};

#pragma pack(push, 1)
struct OrderWireHeader {
  uint16_t magic; // 0x4F52 'OR'
  uint8_t version;
  uint8_t type;
  uint64_t client_order_id;
  uint32_t symbol_id;
};
#pragma pack(pop)

static_assert(sizeof(OrderWireHeader) == 16);
inline constexpr uint16_t kOrderMagic = 0x4F52;
inline constexpr uint8_t kOrderVersion = 1;

struct ClientNewOrder {
  Side side{};
  TimeInForce tif{};
  int32_t price{};
  uint32_t qty{};
};

struct CancelOrderReq {
  // id in header
};

struct ModifyOrderReq {
  Side side{};
  int32_t price{};
  uint32_t qty{};
};

struct ExecReport {
  ExecType type{ExecType::Ack};
  uint64_t client_order_id{};
  uint64_t other_order_id{};
  int32_t price{};
  uint32_t qty{};
  RejectReason reason{RejectReason::None};
};

enum class OrderParseError {
  Ok = 0,
  Truncated,
  BadMagic,
  BadVersion,
  BadType,
};

struct OrderParseResult {
  OrderParseError error{OrderParseError::Ok};
  OrderWireHeader header{};
  ClientNewOrder new_order{};
  ModifyOrderReq modify{};
  std::size_t consumed{0};
};

OrderParseResult parse_order_message(std::span<const std::byte> data);

std::vector<std::byte> encode_new_order(uint64_t client_order_id, uint32_t symbol_id,
                                        const ClientNewOrder& m);
std::vector<std::byte> encode_cancel_order(uint64_t client_order_id, uint32_t symbol_id);
std::vector<std::byte> encode_modify_order(uint64_t client_order_id, uint32_t symbol_id,
                                           const ModifyOrderReq& m);

} // namespace gw
