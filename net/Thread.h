#pragma once

#include <sched.h>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "global.h"
#include "CountDownLatch.h"
namespace rmuduo {

class Thread : public noncopyable {
 public:
  using ThreadFunc = std::function<void()>;

  explicit Thread(ThreadFunc func, const std::string& name = std::string());
  ~Thread();

  void start();
  void join();

  bool started() const { return start_; }

  pid_t tid() const { return tid_; }
  const std::string& name() const { return name_; }

  static int numCreated() { return numCreated_; }

 private:
  void setDefaultName();

  bool start_;
  bool joined_;

  std::shared_ptr<std::thread> thread_;
  pid_t tid_;
  ThreadFunc func_;

  std::string name_;
  static std::atomic_int32_t numCreated_;  // 总的线程创建数量

  CountDownLatch latch_;
};
}  // namespace rmuduo