#pragma once

#include <cstdint>
#include <string>
namespace rmuduo {
class Timestamp {
 public:
  Timestamp();
  explicit Timestamp(int64_t microSecondsSinceEpoch);
  ~Timestamp();

  int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
  static Timestamp now();
  std::string toString() const;
  std::string toFormattedString(bool showMicroseconds = true) const;

  static const int kMicroSecondsPerSecond = 1000 * 1000;
 private:
  int64_t microSecondsSinceEpoch_;
};
}  // namespace rmuduo