#include "Socket.h"

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "InetAddress.h"
#include "Logger.h"

namespace rmuduo {

void Socket::bindAddress(const InetAddress& addr) {
  if (::bind(sockfd_, addr.getSockAddr(), sizeof(*addr.getSockAddr())) != 0) {
    LOG_ERROR("Socket bind sockfd error! errno={}", errno);
  }
}

void Socket::listen() {
  if (::listen(sockfd_, 1024) != 0) {
    LOG_ERROR("Socket listen sockfd error! errno={}", errno);
  }
}

int Socket::accept(InetAddress* peerAddr) {
  sockaddr_in addr;
  socklen_t len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  int connfd =
      ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (connfd >= 0) {
    peerAddr->setSockAddr(addr);
  } else {
    LOG_ERROR("Socket listen sockfd error! errno={}", errno);
  }
  return connfd;
}

void Socket::shutdownWrite() {
  if (::shutdown(sockfd_, SHUT_WR) < 0) {
    LOG_ERROR("Socket listen sockfd error! errno={}", errno);
  }
}

void Socket::setTcpNoDelay(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}
void Socket::setReuseAddr(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}
void Socket::setReusePort(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}
void Socket::setKeepAlive(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

}  // namespace rmuduo