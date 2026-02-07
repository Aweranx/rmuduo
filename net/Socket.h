#pragma once
#include "global.h"
namespace rmuduo {

class InetAddress;
class Socket : public noncopyable {
 public:
  explicit Socket(int sockfd) : sockfd_(sockfd) {}
  ~Socket() { ::close(sockfd_); }

  int fd() const { return sockfd_; }
  void bindAddress(const InetAddress& addr);
  void listen();
  int accept(InetAddress* peerAddr);

  void shutdownWrite();

  void setTcpNoDelay(bool on);
  void setReuseAddr(bool on);
  void setReusePort(bool on);
  void setKeepAlive(bool on);

 private:
  const int sockfd_;
};
}  // namespace rmuduo