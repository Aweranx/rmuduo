#include "EventLoopThread.h"

#include <functional>
#include <mutex>

#include "EventLoop.h"
#include "Thread.h"

namespace rmuduo {
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string& name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != nullptr) {
    loop_->quit();
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop() {
  thread_.start();
  EventLoop* loop = nullptr;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]() { return loop_ != nullptr; });
    loop = loop_;
  }
  return loop;
}

void EventLoopThread::threadFunc() {
  // 将loop直接创建在子线程的栈上
  EventLoop loop; // one loop per thread

  if (callback_) {
    callback_(&loop);
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = &loop;
    cond_.notify_one();
  }

  loop.loop();

  std::lock_guard<std::mutex> lock(mutex_);
  loop_ = nullptr;
}

}  // namespace rmuduo