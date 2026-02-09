#include "Timer.h"

namespace rmuduo {
std::atomic_int64_t Timer::s_numCreated_;

void Timer::restart(Timestamp now) {
  if (repeat_) {
    expiration_ = addTime(now, interval_);
  } else {
    expiration_ = Timestamp::invalid();
  }
}
}  // namespace rmuduo