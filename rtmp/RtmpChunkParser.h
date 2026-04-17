#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <rmuduo/net/Buffer.h>

#include "RtmpMessage.h"
#include "RtmpTypes.h"

namespace rmuduo::rtmp {

class RtmpChunkParser {
 public:
  void setInChunkSize(uint32_t chunk_size) { inChunkSize_ = chunk_size; }
  uint32_t inChunkSize() const { return inChunkSize_; }

  // 以增量方式消费 Buffer。解析出的完整 message 会追加到 messages。
  bool parse(Buffer* buf, std::vector<RtmpMessage>* messages,
             std::string* error_message);

 private:
  struct ChunkStreamState {
    uint32_t timestamp = 0;
    uint32_t timestampDelta = 0;
    uint32_t extendedTimestamp = 0;
    uint32_t messageLength = 0;
    uint8_t typeId = 0;
    uint32_t messageStreamId = 0;
    uint32_t bytesRead = 0;
    std::string payload;
    bool headerInitialized = false;
  };

  bool parseOneChunk(Buffer* buf, std::vector<RtmpMessage>* messages,
                     std::string* error_message);
  bool parseBasicHeader(const char* data, size_t readable_bytes, uint8_t* fmt,
                        uint32_t* csid, size_t* header_size,
                        std::string* error_message) const;
  bool parseMessageHeader(const char* data, size_t readable_bytes, uint8_t fmt,
                          ChunkStreamState* state, size_t* bytes_used,
                          std::string* error_message) const;

  uint32_t inChunkSize_ = kDefaultChunkSize;
  std::unordered_map<uint32_t, ChunkStreamState> chunkStreams_;
};

}  // namespace rmuduo::rtmp
