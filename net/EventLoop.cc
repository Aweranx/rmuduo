#include "EventLoop.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "Channel.h"
#include "CurrentThread.h"
#include "Logger.h"
#include "Poller.h"
#include "Timestamp.h"
namespace rmuduo {

// 防止一个线程创建多个loop
thread_local EventLoop* t_loopInThisThread = nullptr;
const int kPollTimeMs = 10000;

int createEventfd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG_ERROR("eventfd error:{}", errno);
  }
  return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)) {
  LOG_DEBUG("EventLoop created {} in thread {}", (void*)this, threadId_);
  if (t_loopInThisThread) {
    LOG_ERROR("Another EventLoop {} exists in this thread {}", (void*)this, threadId_);
  } else {
    t_loopInThisThread = this;
  }
  // 设置wakeupfd读时间
  wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
  looping_ = true;
  quit_ = false;
  LOG_DEBUG("EventLoop {} start looping", (void*)this);

  while (!quit_) {
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

    eventHandling_ = true;
    for (Channel* channel : activeChannels_) {
      channel->handleEvent(pollReturnTime_);
    }
    eventHandling_ = false;
    // 执行当前EventLoop事件循环需要处理的回调操作
    /*
      IO线程 mainloop accept fd 《= channel subloop
      mainLoop实现注册一个回调cb（需要subloop来执行） wakeup
      subloop后，执行下面的方法，执行mainloop注册的cb
    */
    doPendingFunctors();
  }

  LOG_INFO("EventLoop {} stop looping", (void*)this);
}

void EventLoop::quit() {
  quit_ = false;
  if (!isInLoopThread()) {
    wakeup();
  }
}

void EventLoop::runInLoop(Functor cb) {
  if (isInLoopThread()) {
    cb();
  } else {
    queueInLoop(cb);
  }
}
// 把cb放入队列，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingFunctors_.emplace_back(std::move(cb));
  }
  // 唤醒响应的，需要执行上面回调操作的loop的线程
  // callingPendingFunctors_的意思是：
  // 当前loop正在执行回调，但是loop又有了新的回调，为防止结束回调后又阻塞在poll
  if (!isInLoopThread() || callingPendingFunctors_) {
    wakeup();
  }
}

void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_ERROR("EventLoop::handleRead() reads {} bytes ", n);
  }
}
void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_ERROR("EventLoop::wakeup() write {} bytes ", n);
  }
}

void EventLoop::updateChannel(Channel* channel) {
  poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel) {
  poller_->removeChannel(channel);
}
void EventLoop::hasChannel(Channel* channel) { poller_->hasChannel(channel); }

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    functors.swap(pendingFunctors_);
  }
  for (const Functor& functor : functors) {
    functor();
  }
  callingPendingFunctors_ = false;
}

}  // namespace rmuduo
