#include "Timestamp.h"
#include <chrono>
#include <cstdint>
#include <format>

using namespace rmuduo;

Timestamp Timestamp::now() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    int64_t micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    return Timestamp(micros);
}

std::string Timestamp::toString() const{
    using namespace std::chrono;
    microseconds duration(microSecondsSinceEpoch_);
    sys_time<microseconds> tp(duration);
    return std::format("{:%Y-%m-%d %H:%M:%S}", tp);
}