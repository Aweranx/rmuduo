#pragma once

#include <functional>
#include <memory>

#include "Timestamp.h"
#include "global.h"
namespace rmuduo {
class EventLoop;
class Channel : noncopyable {
 public:
  using EventCallback = std::function<void()>;
  using ReadEventCallback = std::function<void(Timestamp)>;
  Channel(EventLoop* loop, int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);

  void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  void tie(const std::shared_ptr<void>&);
  int fd() const { return fd_; }
  int events() const { return events_; }
  void set_revents(int revt) { revents_ = revt; }

  void enableReading();
  void disableReading();
  void enableWriting();
  void disableWriting();
  void disableAll();

  bool isNonEvent() { return events_ == kNoneEvent; }
  bool isReading() { return events_ == kReadEvent; }
  bool isWriting() { return events_ == kWriteEvent; }

  int index() const { return index_; }
  void set_index(int idx) { index_ = idx; }

  EventLoop* ownerLoop() { return loop_; }
  void remove();

 private:
  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;
  const int fd_;
  int events_;   // fd感兴趣的事件
  int revents_;  // poller返回的事件
  int index_;
  bool eventHandling_;  // 防止析构正在处理事件的channel
  bool added2Loop_;     // channel是否被loop添加

  // 用以绑定如tcpconnection的obj指针，防止channel处理事件时obj被析构
  std::weak_ptr<void> tie_;
  bool tied_;

  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};
}  // namespace rmuduo