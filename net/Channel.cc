#include "Channel.h"

#include <sys/epoll.h>

#include "EventLoop.h"
#include "Logger.h"
#include "Timestamp.h"

using namespace rmuduo;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false),
      eventHandling_(false),
      added2Loop_(false) {}
Channel::~Channel() {}

void Channel::handleEvent(Timestamp receiveTime) {
  if (tied_) {
    auto guard = tie_.lock();
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
  } else {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::tie(const std::shared_ptr<void>& obj) {
  tie_ = obj;
  tied_ = true;
}

void Channel::enableReading() {
  events_ |= kReadEvent;
  update();
}
void Channel::disableReading() {
  events_ &= ~kReadEvent;
  update();
}
void Channel::enableWriting() {
  events_ |= kWriteEvent;
  update();
}
void Channel::disableWriting() {
  events_ &= ~kWriteEvent;
  update();
}
void Channel::disableAll() {
  events_ = kNoneEvent;
  update();
}

void Channel::update() {
  added2Loop_ = true;
  loop_->updateChannel(this);
}

void Channel::remove() {
  added2Loop_ = false;
  loop_->removeChannel(this);
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
  LOG_INFO("channel handleEvent Revents:{}", revents_);
  eventHandling_ = true;
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) closeCallback_();
  }
  if (revents_ & EPOLLERR) {
    if (errorCallback_) errorCallback_();
  }
  if (revents_ & (EPOLLIN | EPOLLPRI)) {
    if (readCallback_) readCallback_(receiveTime);
  }
  if (revents_ & EPOLLOUT) {
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}