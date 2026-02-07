#include "Acceptor.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <functional>

#include "InetAddress.h"
#include "Logger.h"

namespace rmuduo {

static int createNonblocking() {
  int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                        IPPROTO_TCP);
  if (sockfd < 0) {
    LOG_ERROR("Acceptor createNonblocking  sockfd error! errno={}", errno);
  }
  return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr,
                   bool reusePort)
    : loop_(loop),
      acceptScoket_(createNonblocking()),
      acceptChannel_(loop, acceptScoket_.fd()),
      isListening_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
  acceptScoket_.setReuseAddr(true);
  acceptScoket_.setReusePort(reusePort);
  acceptScoket_.bindAddress(listenAddr);

  acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

void Acceptor::listen() {
  isListening_ = true;
  acceptScoket_.listen();
  acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
  InetAddress peerAddr;
  int connfd = acceptScoket_.accept(&peerAddr);
  if (connfd >= 0) {
    if (newConnectionCallback_) {
      // 上层的TCPserver给出， 分发fd给subloop
      newConnectionCallback_(connfd, peerAddr);
    } else {
      ::close(connfd);
    }
  } else {
    LOG_ERROR("Acceptor accept error");
    // 设一个空的fd占位，当fd资源都满了后，就释放这个空fd，把来的lfd接受再立马关闭，然后再接着占位
    if (errno == EMFILE) {
      ::close(idleFd_);
      ::accept(idleFd_, nullptr, nullptr);
      ::close(idleFd_);
      idleFd_ = ::open("dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

}  // namespace rmuduo