#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <string>

namespace rmuduo {
class InetAddress {
 public:
  explicit InetAddress(uint16_t prot = 0, bool loopbackOnly = false);
  InetAddress(const std::string& ip, uint16_t port);
  explicit InetAddress(const sockaddr_in& addr) : addr_(addr) {}

  std::string toIp() const;
  std::string toIpPort() const;
  uint16_t toPort() const;

  const sockaddr* getSockAddr() const {
    return reinterpret_cast<const sockaddr*>(&addr_);
  }
  void setSockAddr(const sockaddr_in& addr) { addr_ = addr; };

 private:
  sockaddr_in addr_;
};
}  // namespace rmuduo