#pragma once

#include "mdf/messages.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace mdf {

struct PacketView {
  uint64_t seq{};
  std::span<const std::byte> bytes;
};

class ReplaySource {
public:
  using PacketHandler = std::function<bool(PacketView)>;

  bool load_file(const std::string& path);
  void load_memory(std::vector<std::byte> data);
  void reset();

  // Deterministic: walks packets in order with no wall-clock delay.
  std::size_t for_each(const PacketHandler& handler) const;
  std::size_t packet_count() const { return packets_.size(); }

  const std::vector<std::byte>& raw() const { return raw_; }

private:
  bool index_packets();

  std::vector<std::byte> raw_;
  std::vector<PacketView> packets_;
};

} // namespace mdf
