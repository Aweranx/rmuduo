#include "Buffer.h"

#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>

namespace rmuduo {

const char Buffer::kCRLF[] = "\r\n";

Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
      readerIndex_(kCheapPrepend),
      writerIndex_(kCheapPrepend) {}

void Buffer::retrieve(size_t len) {
  if (len <= readableBytes()) {
    readerIndex_ += len;
  } else {
    retrieveAll();
  }
}

std::string Buffer::retrieveAsString(size_t len) {
  std::string res(peek(), len);
  retrieve(len);
  return res;
}

void Buffer::ensureWritableBytes(size_t len) {
  if (writableBytes() < len) {
    makeSpace(len);
  }
}

void Buffer::append(const char* data, size_t len) {
  ensureWritableBytes(len);
  std::copy(data, data + len, beginWrite());
  writerIndex_ += len;
}

// 用来查找特定的标识，如\r\n
const char* Buffer::findCRLF() const {
  const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
  return crlf == beginWrite() ? NULL : crlf;
}
const char* Buffer::findCRLF(const char* start) const {
  const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
  return crlf == beginWrite() ? NULL : crlf;
}

// fd -> buffer
// readv允许一次性把数据读到多个不连续的内存区域。
// 数据先填满 vec[0]（Buffer 内部空间），剩下的自动填进 vec[1]（栈空间）。
ssize_t Buffer::readFd(int fd, int* saveErrno) {
  char extrabuf[65536];  // 栈上的内存空间  64k
  struct iovec vec[2];
  const size_t writable = writableBytes();  // 这是Buffer底层缓冲区剩余可写大小
  vec[0].iov_base = begin() + writerIndex_;
  vec[0].iov_len = writable;

  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof(extrabuf);

  const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;  // 一次最多读64k

  const ssize_t n = ::readv(fd, vec, iovcnt);

  if (n < 0) {
    *saveErrno = errno;
  } else if (n <= writable)  // buffer的可写缓冲区够用
  {
    writerIndex_ += n;
  } else  // extrabuf中也写入了数据
  {
    writerIndex_ = buffer_.size();
    append(extrabuf, n - writable);  // writeIndex_开始写n-writable大小的数据
  }
  return n;
}
// buffer -> fd, 由上层逻辑配合调用retrieve(n)
ssize_t Buffer::writeFd(int fd, int* saveErrno) {
  ssize_t n = ::write(fd, peek(), readableBytes());
  if (n < 0) {
    *saveErrno = errno;
  }
  return n;
}

void Buffer::makeSpace(size_t len) {
  // 先考虑总大小是否小于len，小于的话扩容buffer
  // 不然的话将缓冲区前移，覆盖旧数据
  if (writableBytes() + prependableByte() < len + kCheapPrepend) {
    buffer_.resize(writerIndex_ + len);
  } else {
    size_t readable = readableBytes();
    std::copy(begin() + readerIndex_, begin() + writerIndex_,
              begin() + kCheapPrepend);
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend + readable;
  }
}

}  // namespace rmuduo