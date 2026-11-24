#pragma once

// Minimal OSC 1.0 sender over UDP.
// Deliberately free of any Windows / UE4SS header so it can be unit-tested
// on its own. Only loopback destinations (127.0.0.0/8) are accepted: the
// stream never leaves the computer that runs the game.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class OscMessage {
public:
  explicit OscMessage(std::string_view address);

  OscMessage &add(int32_t value);
  OscMessage &add(float value);
  OscMessage &add(std::string_view value);

  // Encoded packet, ready to be sent as one UDP datagram.
  std::vector<char> encode() const;

private:
  std::string address_;
  std::string type_tags_ = ",";
  std::vector<char> args_;
};

class OscSender {
public:
  OscSender() = default;
  ~OscSender();
  OscSender(const OscSender &) = delete;
  OscSender &operator=(const OscSender &) = delete;

  // Returns false if the host is not a loopback address or the socket
  // cannot be created. `error` receives a short English explanation.
  bool open(const std::string &host, uint16_t port, std::string &error);
  void close();
  bool is_open() const { return socket_ != invalid_socket; }

  // Never blocks: if the OS buffer is full the datagram is dropped.
  void send(const OscMessage &message);

  // Exposed for tests.
  static bool is_loopback(const std::string &host);

private:
  static constexpr uintptr_t invalid_socket = ~static_cast<uintptr_t>(0);
  uintptr_t socket_ = invalid_socket;
  uint32_t address_be_ = 0; // IPv4, network byte order
  uint16_t port_be_ = 0;     // network byte order
  bool winsock_started_ = false;
};
