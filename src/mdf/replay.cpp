#include "mdf/replay.hpp"

#include "mdf/recorder.hpp"

#include <cstring>
#include <fstream>

namespace mdf {

bool ReplaySource::load_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  in.seekg(0, std::ios::end);
  const auto sz = static_cast<std::size_t>(in.tellg());
  in.seekg(0, std::ios::beg);
  raw_.resize(sz);
  if (sz && !in.read(reinterpret_cast<char*>(raw_.data()), static_cast<std::streamsize>(sz)))
    return false;
  return index_packets();
}

void ReplaySource::load_memory(std::vector<std::byte> data) {
  raw_ = std::move(data);
  index_packets();
}

void ReplaySource::reset() {
  raw_.clear();
  packets_.clear();
}

bool ReplaySource::index_packets() {
  packets_.clear();
  std::size_t off = 0;
  while (off + sizeof(RecordHeader) <= raw_.size()) {
    RecordHeader h{};
    std::memcpy(&h, raw_.data() + off, sizeof(h));
    off += sizeof(h);
    if (off + h.length > raw_.size())
      return false;
    packets_.push_back(PacketView{h.seq, std::span<const std::byte>(raw_.data() + off, h.length)});
    off += h.length;
  }
  return true;
}

std::size_t ReplaySource::for_each(const PacketHandler& handler) const {
  std::size_t n = 0;
  for (const auto& p : packets_) {
    if (!handler(p))
      break;
    ++n;
  }
  return n;
}

} // namespace mdf
