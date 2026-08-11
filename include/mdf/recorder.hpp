#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace mdf {

#pragma pack(push, 1)
struct RecordHeader {
  uint32_t length;
  uint64_t seq;
};
#pragma pack(pop)

class Recorder {
public:
  bool open(const std::string& path);
  void close();
  bool is_open() const { return out_.is_open(); }

  bool write_packet(uint64_t seq, std::span<const std::byte> packet);
  void flush();

private:
  std::ofstream out_;
};

class MemoryRecorder {
public:
  void write_packet(uint64_t seq, std::span<const std::byte> packet);
  const std::vector<std::byte>& data() const { return data_; }
  void clear() { data_.clear(); }

private:
  std::vector<std::byte> data_;
};

} // namespace mdf
