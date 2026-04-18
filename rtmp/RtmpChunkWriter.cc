#include "RtmpChunkWriter.h"

#include <algorithm>
#include <cstdint>

namespace rmuduo::rtmp {

std::string RtmpChunkWriter::EncodeMessage(const RtmpMessage& message,
                                           uint32_t out_chunk_size) {
  std::string out;
  size_t payload_offset = 0;

  appendBasicHeader(&out, 0, message.chunkStreamId);
  appendMessageHeader(&out, message);

  while (payload_offset < message.payload.size()) {
    const size_t remaining = message.payload.size() - payload_offset;
    const size_t chunk_bytes = std::min<size_t>(remaining, out_chunk_size);
    out.append(message.payload.data() + static_cast<std::ptrdiff_t>(payload_offset),
               chunk_bytes);
    payload_offset += chunk_bytes;

    if (payload_offset < message.payload.size()) {
      appendBasicHeader(&out, 3, message.chunkStreamId);
    }
  }

  return out;
}

void RtmpChunkWriter::appendBasicHeader(std::string* out, uint8_t fmt,
                                        uint32_t csid) {
  if (csid < 64) {
    out->push_back(static_cast<char>((fmt << 6) | csid));
    return;
  }

  if (csid < 320) {
    out->push_back(static_cast<char>(fmt << 6));
    out->push_back(static_cast<char>(csid - 64));
    return;
  }

  out->push_back(static_cast<char>((fmt << 6) | 1));
  const uint32_t value = csid - 64;
  out->push_back(static_cast<char>(value & 0xFF));
  out->push_back(static_cast<char>((value >> 8) & 0xFF));
}

void RtmpChunkWriter::appendMessageHeader(std::string* out,
                                          const RtmpMessage& message) {
  appendUint24BE(out, std::min<uint32_t>(message.timestamp, 0x00FFFFFF));
  appendUint24BE(out, message.messageLength);
  out->push_back(static_cast<char>(message.typeId));
  appendUint32LE(out, message.messageStreamId);

  if (message.timestamp >= 0x00FFFFFF) {
    appendUint32BE(out, message.timestamp);
  }
}

void RtmpChunkWriter::appendUint24BE(std::string* out, uint32_t value) {
  out->push_back(static_cast<char>((value >> 16) & 0xFF));
  out->push_back(static_cast<char>((value >> 8) & 0xFF));
  out->push_back(static_cast<char>(value & 0xFF));
}

void RtmpChunkWriter::appendUint32BE(std::string* out, uint32_t value) {
  out->push_back(static_cast<char>((value >> 24) & 0xFF));
  out->push_back(static_cast<char>((value >> 16) & 0xFF));
  out->push_back(static_cast<char>((value >> 8) & 0xFF));
  out->push_back(static_cast<char>(value & 0xFF));
}

void RtmpChunkWriter::appendUint32LE(std::string* out, uint32_t value) {
  out->push_back(static_cast<char>(value & 0xFF));
  out->push_back(static_cast<char>((value >> 8) & 0xFF));
  out->push_back(static_cast<char>((value >> 16) & 0xFF));
  out->push_back(static_cast<char>((value >> 24) & 0xFF));
}

}  // namespace rmuduo::rtmp
