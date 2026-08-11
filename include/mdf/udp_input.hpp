#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace mdf {

class UdpReceiver {
public:
  using PacketHandler = std::function<void(std::span<const std::byte>)>;

  UdpReceiver();
  ~UdpReceiver();

  UdpReceiver(const UdpReceiver&) = delete;
  UdpReceiver& operator=(const UdpReceiver&) = delete;

  bool bind(const std::string& host, uint16_t port);
  void close();

  // Blocking receive into buf; returns bytes read or -1 on error / closed.
  int recv(std::byte* buf, std::size_t len);

  bool poll(const PacketHandler& handler, int timeout_ms = 0);

private:
  void* sock_{nullptr}; // SOCKET stored as opaque to keep header clean on non-Windows headers
  bool ready_{false};
};

class UdpSender {
public:
  UdpSender();
  ~UdpSender();

  UdpSender(const UdpSender&) = delete;
  UdpSender& operator=(const UdpSender&) = delete;

  bool connect(const std::string& host, uint16_t port);
  void close();
  bool send(std::span<const std::byte> packet);

private:
  void* sock_{nullptr};
  bool ready_{false};
};

} // namespace mdf
