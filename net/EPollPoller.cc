#include "EPollPoller.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Timestamp.h"

using namespace rmuduo;

// channel未添加到poller中
const int kNew = -1;  // channel的成员index = -1
// channel已添加
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
  if (epollfd_ < 0) {
    LOG_ERROR("EPollPoller epoll_create error! errno = {}", errno);
  }
}
EPollPoller::~EPollPoller() { ::close(epollfd_); }

// 将就绪事件list返回给EventLoop
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
  LOG_DEBUG("poll called");

  int numEvents =
      ::epoll_wait(epollfd_, &*events_.begin(), events_.size(), timeoutMs);
  int saveErrno = errno;
  Timestamp now(Timestamp::now());
  if (numEvents > 0) {
    LOG_INFO("{} events happend", numEvents);
    fillActivateChannels(numEvents, activeChannels);
    if (numEvents == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    LOG_DEBUG("{} timeout", __FUNCTION__);
  } else {
    if (saveErrno != EINTR) {
      errno = saveErrno;
      LOG_ERROR("EPollPoller poll error");
    }
  }
  return now;
}
void EPollPoller::fillActivateChannels(int numEvents,
                                       ChannelList* activeChannels) const {
  for (int i = 0; i < numEvents; ++i) {
    Channel* channle = static_cast<Channel*>(events_[i].data.ptr);
    channle->set_revents(events_[i].events);
    activeChannels->push_back(channle);
  }
}

void EPollPoller::updateChannel(Channel* channel) {
  const int index = channel->index();
  const int fd = channel->fd();
  LOG_INFO("func={}, fd={}, event={}, index={}", __FUNCTION__, fd,
           channel->events(), index);
  if (index == kNew || index == kDeleted) {
    if (index == kNew) {
      channels_[fd] = channel;
    }

    channel->set_index(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    if (channel->isNonEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}
void EPollPoller::removeChannel(Channel* channel) {
  int fd = channel->fd();
  channels_.erase(fd);
  int index = channel->index();
  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel) {
  epoll_event evt;
  memset(&evt, 0, sizeof(evt));
  evt.events = channel->events();
  evt.data.ptr = channel;
  int fd = channel->fd();

  if (::epoll_ctl(epollfd_, operation, fd, &evt) < 0) {
    LOG_ERROR("epoll_ctl error:{}!", errno);
  }
}