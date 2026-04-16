#include "RtmpHandshake.h"

#include <array>
#include <cstdint>
#include <random>

#include <rmuduo/net/Buffer.h>

#include "RtmpTypes.h"

namespace rmuduo::rtmp {

namespace {

constexpr size_t kHandshakePacketSize = 1536;
constexpr size_t kC0C1Size = 1 + kHandshakePacketSize;

}  // namespace

bool RtmpHandshake::process(Buffer* buf, std::string* response,
                            std::string* error_message) {
  while (true) {
    if (state_ == HandshakeState::kWaitC0C1) {
      // 第一阶段必须等到完整的 C0C1 到齐，再校验版本并回 S0S1S2。
      if (buf->readableBytes() < kC0C1Size) {
        return true;
      }

      const char* data = buf->peek();
      if (static_cast<uint8_t>(data[0]) != kVersion) {
        if (error_message != nullptr) {
          *error_message = "unsupported rtmp version";
        }
        return false;
      }

      if (response != nullptr) {
        response->append(buildS0S1S2(data + 1));
      }
      buf->retrieve(kC0C1Size);
      state_ = HandshakeState::kWaitC2;
      continue;
    }

    if (state_ == HandshakeState::kWaitC2) {
      // 第二阶段等客户端把 C2 发完，服务端握手才真正结束。
      if (buf->readableBytes() < kHandshakePacketSize) {
        return true;
      }

      buf->retrieve(kHandshakePacketSize);
      state_ = HandshakeState::kHandshakeDone;
      return true;
    }

    return true;
  }
}

bool RtmpHandshake::isDone() const {
  return state_ == HandshakeState::kHandshakeDone;
}

std::string RtmpHandshake::buildS0S1S2(const char* c1) const {
  std::string response;
  response.resize(1 + kHandshakePacketSize + kHandshakePacketSize);

  response[0] = static_cast<char>(kVersion);

  // S1: 4 字节时间 + 4 字节零值 + 1528 字节随机数。
  std::fill(response.begin() + 1, response.begin() + 9, '\0');

  std::random_device rd;
  for (size_t i = 0; i < kHandshakePacketSize - 8; ++i) {
    response[9 + i] = static_cast<char>(rd());
  }

  // S2 直接回显客户端的 C1 内容，这是 simple handshake 的标准做法。
  std::copy(c1, c1 + kHandshakePacketSize,
            response.begin() + 1 + kHandshakePacketSize);
  return response;
}

}  // namespace rmuduo::rtmp
