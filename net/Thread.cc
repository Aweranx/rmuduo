#include "Thread.h"

#include <format>
#include <memory>
#include <thread>

#include "CurrentThread.h"

namespace rmuduo {

std::atomic_int32_t Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
    : start_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)),
      name_(name),
      latch_(1) {
  setDefaultName();
}
Thread::~Thread() {
  if (start_ && !joined_) {
    thread_->detach();
  }
}

void Thread::start() {
  start_ = true;
  thread_ = std::shared_ptr<std::thread>(new std::thread([this]() {
    tid_ = CurrentThread::tid();
    // 使用CountDownLatch到时器进行同步，使得这个tid_获得后，主线程才可执行
    latch_.countDown();
    func_();
  }));
  latch_.wait();
}

void Thread::join() {
  joined_ = true;
  thread_->join();
}

void Thread::setDefaultName() {
  int num = ++numCreated_;
  if (name_.empty()) {
    name_ = std::format("Thread{}", num);
  }
}
}  // namespace rmuduo