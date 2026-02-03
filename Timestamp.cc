#include "Timestamp.h"

#include <chrono>
#include <cstdint>
#include <format>

using namespace rmuduo;

Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}
Timestamp::~Timestamp() {}

Timestamp Timestamp::now() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  int64_t micros =
      std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  return Timestamp(micros);
}

std::string Timestamp::toString() const {
  int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
  int64_t miscroseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
  return std::format("{}, {:06}", seconds, miscroseconds);
}

std::string Timestamp::toFormattedString(bool showMicroseconds) const {
  using namespace std::chrono;
  microseconds duration(microSecondsSinceEpoch_);
  sys_time<microseconds> tp(duration);
  auto current_zone = locate_zone("Asia/Shanghai");
  auto local_tp = current_zone->to_local(tp);
  if (showMicroseconds) {
    return std::format("{:%Y-%m-%d %H:%M:%S}.{:06}", floor<seconds>(local_tp),
                       microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
  } else {
    return std::format("{:%Y-%m-%d %H:%M:%S}", floor<seconds>(local_tp));
  }
}
