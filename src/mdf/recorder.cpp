#include "mdf/recorder.hpp"

#include <cstring>

namespace mdf {

bool Recorder::open(const std::string& path) {
  out_.open(path, std::ios::binary | std::ios::trunc);
  return out_.is_open();
}

void Recorder::close() {
  if (out_.is_open())
    out_.close();
}

bool Recorder::write_packet(uint64_t seq, std::span<const std::byte> packet) {
  if (!out_.is_open())
    return false;
  RecordHeader h{};
  h.length = static_cast<uint32_t>(packet.size());
  h.seq = seq;
  out_.write(reinterpret_cast<const char*>(&h), sizeof(h));
  out_.write(reinterpret_cast<const char*>(packet.data()), static_cast<std::streamsize>(packet.size()));
  return static_cast<bool>(out_);
}

void Recorder::flush() {
  if (out_.is_open())
    out_.flush();
}

void MemoryRecorder::write_packet(uint64_t seq, std::span<const std::byte> packet) {
  RecordHeader h{};
  h.length = static_cast<uint32_t>(packet.size());
  h.seq = seq;
  const auto* hp = reinterpret_cast<const std::byte*>(&h);
  data_.insert(data_.end(), hp, hp + sizeof(h));
  data_.insert(data_.end(), packet.begin(), packet.end());
}

} // namespace mdf
