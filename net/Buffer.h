#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rmuduo {

class Buffer {
 public:
  // 预留头部位置，在填充完业务数据后，如要填写4byte头部，仅需将指针向前移四位填写即可
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;

  explicit Buffer(size_t initialSize = kInitialSize);
  ~Buffer() = default;

  size_t readableBytes() const { return writerIndex_ - readerIndex_; }
  size_t writableBytes() const { return buffer_.size() - writerIndex_; }
  size_t prependableByte() const { return readerIndex_; }
  // 获取可读数据的起始地址
  const char* peek() const { return begin() + readerIndex_; }

  void retrieve(size_t len);
  void retrieveAll() { readerIndex_ = writerIndex_ = kCheapPrepend; }
  void retrieveUntil(const char* end) { retrieve(end - peek()); }

  std::string retrieveAllAsString() {
    return retrieveAsString(readableBytes());
  }
  std::string retrieveAsString(size_t len);
  void ensureWritableBytes(size_t len);
  // 将数据加入缓冲区
  void append(std::string_view str) { append(str.data(), str.size()); }
  void append(const char* data, size_t len);

  // 用来查找特定的标识，如\r\n
  const char* findCRLF() const;
  const char* findCRLF(const char* start) const;

  char* beginWrite() { return begin() + writerIndex_; }
  const char* beginWrite() const { return begin() + writerIndex_; }

  ssize_t readFd(int fd, int* saveErrno);
  ssize_t writeFd(int fd, int* saveErrno);

 private:
  char* begin() { return &*buffer_.begin(); }
  const char* begin() const { return &*buffer_.begin(); }

  void makeSpace(size_t len);

  std::vector<char> buffer_;
  size_t readerIndex_;
  size_t writerIndex_;

  static const char kCRLF[];
};

}  // namespace rmuduo