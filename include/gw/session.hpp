#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace gw {

enum class StreamChannel : uint8_t { MarketData = 1, Order = 2 };

class SessionRecorder {
public:
  void write(StreamChannel channel, std::span<const std::byte> packet) {
    data_.push_back(static_cast<std::byte>(channel));
    const uint32_t len = static_cast<uint32_t>(packet.size());
    const auto* lp = reinterpret_cast<const std::byte*>(&len);
    data_.insert(data_.end(), lp, lp + sizeof(len));
    data_.insert(data_.end(), packet.begin(), packet.end());
  }

  const std::vector<std::byte>& data() const { return data_; }
  void clear() { data_.clear(); }

private:
  std::vector<std::byte> data_;
};

struct SessionPacket {
  StreamChannel channel{};
  std::span<const std::byte> bytes;
};

inline std::vector<SessionPacket> parse_session(std::span<const std::byte> data) {
  std::vector<SessionPacket> out;
  std::size_t off = 0;
  while (off + 1 + sizeof(uint32_t) <= data.size()) {
    const auto channel = static_cast<StreamChannel>(std::to_integer<uint8_t>(data[off]));
    ++off;
    uint32_t len = 0;
    std::memcpy(&len, data.data() + off, sizeof(len));
    off += sizeof(len);
    if (off + len > data.size())
      break;
    out.push_back(SessionPacket{channel, data.subspan(off, len)});
    off += len;
  }
  return out;
}

} // namespace gw
