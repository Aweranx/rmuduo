#pragma once

#include <string>

#include "RtmpTypes.h"

namespace rmuduo {
class Buffer;
}

namespace rmuduo::rtmp {

class RtmpHandshake {
 public:
  RtmpHandshake() = default;

  // 以增量方式消费输入缓冲区。
  // 返回 false 表示协议错误；true 表示“当前状态合法”，可能还需要继续收包。
  bool process(Buffer* buf, std::string* response, std::string* error_message);
  bool isDone() const;
  HandshakeState state() const { return state_; }

 private:
  // 服务端返回的握手包：S0 + S1 + S2。
  std::string buildS0S1S2(const char* c1) const;

  HandshakeState state_ = HandshakeState::kWaitC0C1;
};

}  // namespace rmuduo::rtmp
