#pragma once

#include <cstdint>
#include <string>

namespace rmuduo::rtmp {

struct RtmpMessage {
  uint32_t timestamp = 0;
  uint32_t messageLength = 0;
  uint8_t typeId = 0;
  uint32_t messageStreamId = 0;
  uint32_t chunkStreamId = 0;
  std::string payload;
};

}  // namespace rmuduo::rtmp
