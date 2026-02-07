#pragma once

#include <sys/epoll.h>

#include <vector>

#include "Poller.h"
#include "Timestamp.h"
namespace rmuduo {
class EPollPoller : public rmuduo::Poller {
 public:
  EPollPoller(EventLoop* loop);
  ~EPollPoller() override;

  Timestamp poll(int timeoutMs, ChannelList* activeChannele) override;
  void updateChannel(Channel* channel) override;
  void removeChannel(Channel* channel) override;

 private:
  // Eventlist的初始长度
  static const int kInitEventListSize = 16;

  void fillActivateChannels(int numEvents, ChannelList* activeChannels) const;
  void update(int operation, Channel* channel);

  using EventList = std::vector<epoll_event>;
  int epollfd_;
  // 接收epoll_wait返回事件
  EventList events_;
};
}  // namespace rmuduo