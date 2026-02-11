#pragma once

#include <any>
#include <atomic>
#include <memory>

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Timestamp.h"
#include "global.h"
namespace rmuduo {

class Channel;
class EventLoop;
class Socket;
/*
TcpServer => Acceptor => 有一个新用户连接，通过accept拿到connfd
=> TcpConnection设置回调 => Channel => Poller => Channel的回调操作
*/
class TcpConnection : public noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

  TcpConnection(EventLoop *loop, const std::string &name, int sockfd,
                const InetAddress &localAddr, const InetAddress &peerAddr);
  ~TcpConnection();

  EventLoop *getLoop() const { return loop_; }
  const std::string &name() const { return name_; }
  const InetAddress &localAddress() const { return localAddr_; }
  const InetAddress &peerAddress() const { return peerAddr_; }
  bool connected() const { return state_ == kConnected; }
  void setState(StateE s) { state_ = s; }

  // 用户用的接口
  void send(const std::string &message);
  void shutdown();
  void setTcpNoDelay(bool on);

  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }
  // 待发送数据堆积过多时的回调处理
  void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                size_t highWaterMark) {
    highWaterMarkCallback_ = cb;
    highWaterMark_ = highWaterMark;
  }
  void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

  void connectEstablished();
  void connectDestroyed();

  void setContext(const std::any &context) { context_ = context; }
  const std::any &getContext() const { return context_; }

  std::any *getMutableContext() { return &context_; }

private:
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();

  void sendInLoop(const void *message, size_t len);
  void shutdownInLoop();

  EventLoop *loop_; // 链接所属的subloop
  const std::string name_;
  std::atomic_int state_;
  bool reading_;

  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;

  const InetAddress localAddr_;
  const InetAddress peerAddr_;

  ConnectionCallback connectionCallback_;       // 有新连接时的回调
  MessageCallback messageCallback_;             // 有读写消息时的回调
  WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
  CloseCallback closeCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;
  size_t highWaterMark_;

  Buffer inputBuffer_;
  Buffer outputBuffer_;
  std::any context_;
};

} // namespace rmuduo