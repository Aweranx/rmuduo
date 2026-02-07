#pragma once

#include <condition_variable>
#include <mutex>

#include "global.h"

namespace rmuduo {

class CountDownLatch : public noncopyable {
 public:
  explicit CountDownLatch(int count);  // 倒数几次
  void wait();
  void countDown();

  int getCount();

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  int count_;
};

}  // namespace rmuduo