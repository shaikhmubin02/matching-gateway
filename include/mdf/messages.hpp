#pragma once

#include <cstdint>
#include <cstring>
#include <variant>
#include <vector>

namespace mdf {

enum class MsgType : uint8_t {
  Add = 1,
  Modify = 2,
  Cancel = 3,
  Trade = 4,
  Snapshot = 5,
  Heartbeat = 6,
};

enum class Side : uint8_t { Bid = 0, Ask = 1 };

struct AddMsg {
  uint64_t order_id{};
  Side side{};
  int64_t price{};
  uint32_t qty{};
};

struct ModifyMsg {
  uint64_t order_id{};
  int64_t price{};
  uint32_t qty{};
};

struct CancelMsg {
  uint64_t order_id{};
};

struct TradeMsg {
  uint64_t trade_id{};
  int64_t price{};
  uint32_t qty{};
  Side aggressor{};
};

struct SnapshotLevel {
  Side side{};
  int64_t price{};
  uint32_t qty{};
};

struct SnapshotMsg {
  uint32_t level_count{};
  uint32_t checksum{};
  std::vector<SnapshotLevel> levels;
};

struct HeartbeatMsg {};

using Payload = std::variant<AddMsg, ModifyMsg, CancelMsg, TradeMsg, SnapshotMsg, HeartbeatMsg>;

struct Event {
  MsgType type{};
  uint64_t seq{};
  uint32_t symbol_id{};
  Payload payload;
};

#pragma pack(push, 1)
struct WireHeader {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint64_t seq;
  uint32_t symbol_id;
};
#pragma pack(pop)

static_assert(sizeof(WireHeader) == 16);

inline constexpr uint16_t kMagic = 0x4D44; // 'MD'
inline constexpr uint8_t kVersion = 1;
inline constexpr size_t kHeaderSize = sizeof(WireHeader);
inline constexpr size_t kMaxPacket = 4096;
inline constexpr size_t kMaxSnapshotLevels = 256;

} // namespace mdf
