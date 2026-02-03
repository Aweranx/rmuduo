#pragma once

#include <memory>
#include <mutex>
namespace rmuduo {
class noncopyable {
 public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

template <typename T>
class Singleton {
 public:
  static std::shared_ptr<T> GetInstance() {
    static std::once_flag flag;
    // make_shared无法访问T的私有构造函数
    std::call_once(flag, [&]() { instance_ = std::shared_ptr<T>(new T); });
    return instance_;
  }

 protected:
  Singleton() = default;
  ~Singleton() = default;
  Singleton(const Singleton&) = delete;
  Singleton& operator=(const Singleton&) = delete;
  static std::shared_ptr<T> instance_;
};
template <typename T>
std::shared_ptr<T> Singleton<T>::instance_ = nullptr;
}  // namespace rmuduo