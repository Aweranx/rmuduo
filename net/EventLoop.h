#pragma once

#include <any>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "Callbacks.h"
#include "Channel.h"
#include "CurrentThread.h"
#include "Logger.h"
#include "Poller.h"
#include "Timestamp.h"
#include "global.h"

namespace rmuduo {
class EventLoop : public noncopyable {
 public:
  using Functor = std::function<void()>;
  EventLoop();
  ~EventLoop();

  void loop();
  void quit();

  Timestamp pollReturnTime() { return pollReturnTime_; }
  // 在当前loop执行cb
  void runInLoop(Functor cb);
  // 把cb放入队列，唤醒loop执行cb
  void queueInLoop(Functor cb);
  // 唤醒loop所在线程
  void wakeup();

  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  void hasChannel(Channel* channel);

  // 判断EventLoop对象是否在自己的线程里边
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  bool eventHandling() const { return eventHandling_; }

  // add code timers

 private:
  using ChannelList = std::vector<Channel*>;

  void handleRead();         // wakeup
  void doPendingFunctors();  // 执行回调

  std::atomic_bool looping_;
  std::atomic_bool quit_;
  std::atomic_bool eventHandling_;

  const pid_t threadId_;
  Timestamp pollReturnTime_;

  std::unique_ptr<Poller> poller_;
  // add code timerqueue

  int wakeupFd_;
  std::unique_ptr<Channel> wakeupChannel_;

  ChannelList activeChannels_;

  std::atomic_bool callingPendingFunctors_;
  std::vector<Functor> pendingFunctors_;
  std::mutex mutex_;
};
}  // namespace rmuduo