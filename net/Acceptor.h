#pragma once

#include <functional>

#include "Channel.h"
#include "Socket.h"
#include "global.h"
namespace rmuduo {

class EventLoop;
class InetAddress;

class Acceptor : public noncopyable {
 public:
  using NewConnectionCallback = std::function<void(int, const InetAddress&)>;
  Acceptor(EventLoop* loop, const InetAddress& listenAddr,
           bool reusePort = true);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback& cb) {
    newConnectionCallback_ = cb;
  }

  bool isListening() const { return isListening_; }
  void listen();

 private:
  void handleRead();

  EventLoop* loop_;  // 用户创建的base loop
  Socket acceptScoket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool isListening_;
  int idleFd_;
};

}  // namespace rmuduo