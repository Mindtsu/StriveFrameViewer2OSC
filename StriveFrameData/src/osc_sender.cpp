#include "osc_sender.h"

#include <cstring>

#ifdef _WIN32
// winsock2.h must be included before windows.h (pulled in by UE4SS headers
// elsewhere), so this file stays free of any other project header.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")
using native_socket = SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using native_socket = int;
#endif

namespace {
  void append_padded_string(std::vector<char> &out, std::string_view text) {
    out.insert(out.end(), text.begin(), text.end());
    // OSC strings are null-terminated and padded to a multiple of 4 bytes.
    const size_t padding = 4 - (text.size() % 4);
    out.insert(out.end(), padding, '\0');
  }

  void append_be32(std::vector<char> &out, uint32_t value) {
    out.push_back(static_cast<char>((value >> 24) & 0xFF));
    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>(value & 0xFF));
  }
} // namespace

// ############################################################
// OscMessage

OscMessage::OscMessage(std::string_view address) : address_(address) {}

OscMessage &OscMessage::add(int32_t value) {
  type_tags_ += 'i';
  append_be32(args_, static_cast<uint32_t>(value));
  return *this;
}

OscMessage &OscMessage::add(float value) {
  type_tags_ += 'f';
  uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  append_be32(args_, bits);
  return *this;
}

OscMessage &OscMessage::add(std::string_view value) {
  type_tags_ += 's';
  append_padded_string(args_, value);
  return *this;
}

std::vector<char> OscMessage::encode() const {
  std::vector<char> out;
  out.reserve(address_.size() + type_tags_.size() + args_.size() + 8);
  append_padded_string(out, address_);
  append_padded_string(out, type_tags_);
  out.insert(out.end(), args_.begin(), args_.end());
  return out;
}

// ############################################################
// OscSender

bool OscSender::is_loopback(const std::string &host) {
  in_addr addr{};
  if (inet_pton(AF_INET, host.c_str(), &addr) != 1) return false;
  const uint32_t host_order = ntohl(addr.s_addr);
  return (host_order >> 24) == 127;
}

bool OscSender::open(const std::string &requested_host, uint16_t port, std::string &error) {
  close();

  const std::string host = (requested_host == "localhost") ? "127.0.0.1" : requested_host;
  if (!is_loopback(host)) {
    error = "host '" + requested_host + "' is not a loopback address (127.x.x.x)";
    return false;
  }

#ifdef _WIN32
  WSADATA wsa_data{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    error = "WSAStartup failed";
    return false;
  }
  winsock_started_ = true;
#endif

  const native_socket sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
  if (sock == INVALID_SOCKET) {
#else
  if (sock < 0) {
#endif
    error = "could not create the UDP socket";
    close();
    return false;
  }

  // Non-blocking: the game thread must never wait on the network.
#ifdef _WIN32
  u_long non_blocking = 1;
  ioctlsocket(sock, FIONBIO, &non_blocking);
#else
  fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif

  in_addr addr{};
  inet_pton(AF_INET, host.c_str(), &addr);
  address_be_ = addr.s_addr;
  port_be_ = htons(port);
  socket_ = static_cast<uintptr_t>(sock);
  return true;
}

void OscSender::close() {
  if (socket_ != invalid_socket) {
#ifdef _WIN32
    closesocket(static_cast<native_socket>(socket_));
#else
    ::close(static_cast<native_socket>(socket_));
#endif
    socket_ = invalid_socket;
  }
#ifdef _WIN32
  if (winsock_started_) {
    WSACleanup();
    winsock_started_ = false;
  }
#endif
}

OscSender::~OscSender() { close(); }

void OscSender::send(const OscMessage &message) {
  if (socket_ == invalid_socket) return;

  const std::vector<char> packet = message.encode();

  sockaddr_in destination{};
  destination.sin_family = AF_INET;
  destination.sin_port = port_be_;
  destination.sin_addr.s_addr = address_be_;

  // Errors are ignored on purpose (nobody listening, buffer full...).
  ::sendto(static_cast<native_socket>(socket_), packet.data(), static_cast<int>(packet.size()), 0,
           reinterpret_cast<const sockaddr *>(&destination), sizeof(destination));
}
