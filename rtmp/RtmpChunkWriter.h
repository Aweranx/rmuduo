#pragma once

#include <string>

#include "RtmpMessage.h"

namespace rmuduo::rtmp {

class RtmpChunkWriter {
 public:
  static std::string EncodeMessage(const RtmpMessage& message,
                                   uint32_t out_chunk_size);

 private:
  static void appendBasicHeader(std::string* out, uint8_t fmt, uint32_t csid);
  static void appendMessageHeader(std::string* out, const RtmpMessage& message);
  static void appendUint24BE(std::string* out, uint32_t value);
  static void appendUint32BE(std::string* out, uint32_t value);
  static void appendUint32LE(std::string* out, uint32_t value);
};

}  // namespace rmuduo::rtmp
