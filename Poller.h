#pragma once

#include <unordered_map>
#include <vector>

#include "Timestamp.h"
#include "global.h"

namespace rmuduo {
class EventLoop;
class Channel;
class Poller : noncopyable {
 public:
  using ChannelList = std::vector<Channel*>;
  Poller(EventLoop* loop);
  virtual ~Poller() = default;

  // epoll_wait
  virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
  virtual void updateChannel(Channel* channel) = 0;
  virtual void removeChannel(Channel* channel) = 0;

  virtual bool hasChannel(Channel* channel) const;

  // 根据环境变量来实例化epoll和poll
  static Poller* newDefaultPoller(EventLoop* loop);

 protected:
  // fd to Channel
  using ChannelMap = std::unordered_map<int, Channel*>;
  ChannelMap channels_;

 private:
  EventLoop* ownerLoop_;
};
}  // namespace rmuduo