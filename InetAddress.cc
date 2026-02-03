#include "InetAddress.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include "Logger.h"
#include <sys/socket.h>

#include <cstdint>
#include <cstring>
#include <format>
#include <string>

using namespace rmuduo;

InetAddress::InetAddress(uint16_t port, bool loopbackOnly) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  in_addr_t ip = loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY;
  addr_.sin_addr.s_addr = htonl(ip);
  addr_.sin_port = ::htons(port);
}

InetAddress::InetAddress(const std::string& ip, uint16_t port) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr.s_addr) != 1) {
    LOG_ERROR("InetAddress::inet_pton() error!");
  }
  addr_.sin_port = ::htons(port);
}

std::string InetAddress::toIp() const {
  char buf[64] = {0};
  if (::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf)) == nullptr) {
    LOG_ERROR("InetAddress::inet_ntop() error!");
  }
  return std::string(buf);
}

uint16_t InetAddress::toPort() const { return ::ntohs(addr_.sin_port); }

std::string InetAddress::toIpPort() const {
  std::string ip = toIp();
  return std::format("{}:{}", ip, toPort());
}
