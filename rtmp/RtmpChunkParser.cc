#include "RtmpChunkParser.h"

#include <algorithm>
#include <cstdint>

#include "RtmpTypes.h"

namespace rmuduo::rtmp {

namespace {

uint32_t ReadUint24BE(const char* data) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(data[0])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 8) |
         static_cast<uint32_t>(static_cast<uint8_t>(data[2]));
}

uint32_t ReadUint32BE(const char* data) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(data[0])) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 8) |
         static_cast<uint32_t>(static_cast<uint8_t>(data[3]));
}

uint32_t ReadUint32LE(const char* data) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(data[3])) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 8) |
         static_cast<uint32_t>(static_cast<uint8_t>(data[0]));
}

}  // namespace

bool RtmpChunkParser::parse(Buffer* buf, std::vector<RtmpMessage>* messages,
                            std::string* error_message) {
  while (buf->readableBytes() > 0) {
    const size_t before = buf->readableBytes();
    if (!parseOneChunk(buf, messages, error_message)) {
      return false;
    }

    // 本轮没有消耗任何数据，说明只是半包，等下次继续。
    if (buf->readableBytes() == before) {
      return true;
    }
  }

  return true;
}

bool RtmpChunkParser::parseOneChunk(Buffer* buf,
                                    std::vector<RtmpMessage>* messages,
                                    std::string* error_message) {
  const char* data = buf->peek();
  const size_t readable = buf->readableBytes();

  uint8_t fmt = 0;
  uint32_t csid = 0;
  size_t basic_header_size = 0;
  if (!parseBasicHeader(data, readable, &fmt, &csid, &basic_header_size,
                        error_message)) {
    return false;
  }
  if (basic_header_size == 0) {
    return true;
  }

  auto& state = chunkStreams_[csid];
  size_t offset = basic_header_size;

  // 当前阶段先支持最常见的 11 字节完整头（fmt=0）和续片头（fmt=3）。
  if (fmt == kChunkHeaderType0) {
    if (readable < offset + 11) {
      return true;
    }

    const uint32_t timestamp = ReadUint24BE(data + offset);
    const uint32_t message_length = ReadUint24BE(data + offset + 3);
    const uint8_t type_id = static_cast<uint8_t>(data[offset + 6]);
    const uint32_t message_stream_id = ReadUint32LE(data + offset + 7);
    offset += 11;

    if (timestamp == 0x00FFFFFF) {
      if (readable < offset + 4) {
        return true;
      }
      state.timestamp = ReadUint32BE(data + offset);
      offset += 4;
    } else {
      state.timestamp = timestamp;
    }

    state.timestampDelta = 0;
    state.messageLength = message_length;
    state.typeId = type_id;
    state.messageStreamId = message_stream_id;
    state.bytesRead = 0;
    state.payload.assign(message_length, '\0');
    state.headerInitialized = true;
  } else if (fmt == kChunkHeaderType3) {
    if (!state.headerInitialized) {
      if (error_message != nullptr) {
        *error_message = "received fmt=3 chunk before first fmt=0 header";
      }
      return false;
    }
  } else {
    if (error_message != nullptr) {
      *error_message = "fmt=1/2 chunk headers are not implemented yet";
    }
    return false;
  }

  if (!state.headerInitialized) {
    return true;
  }

  const uint32_t remaining = state.messageLength - state.bytesRead;
  const uint32_t chunk_payload = std::min(remaining, inChunkSize_);
  if (readable < offset + chunk_payload) {
    return true;
  }

  std::copy(data + offset, data + offset + chunk_payload,
            state.payload.begin() + static_cast<std::ptrdiff_t>(state.bytesRead));
  state.bytesRead += chunk_payload;
  buf->retrieve(offset + chunk_payload);

  if (state.bytesRead == state.messageLength) {
    messages->push_back(RtmpMessage{
        .timestamp = state.timestamp,
        .messageLength = state.messageLength,
        .typeId = state.typeId,
        .messageStreamId = state.messageStreamId,
        .chunkStreamId = csid,
        .payload = state.payload,
    });
    state.bytesRead = 0;
    state.payload.clear();
  }

  return true;
}

bool RtmpChunkParser::parseBasicHeader(const char* data, size_t readable_bytes,
                                       uint8_t* fmt, uint32_t* csid,
                                       size_t* header_size,
                                       std::string* error_message) const {
  if (readable_bytes < 1) {
    *header_size = 0;
    return true;
  }

  const uint8_t first = static_cast<uint8_t>(data[0]);
  *fmt = first >> 6;
  const uint8_t csid_mark = first & 0x3F;

  if (csid_mark >= 2) {
    *csid = csid_mark;
    *header_size = 1;
    return true;
  }

  if (csid_mark == 0) {
    if (readable_bytes < 2) {
      *header_size = 0;
      return true;
    }
    *csid = 64 + static_cast<uint8_t>(data[1]);
    *header_size = 2;
    return true;
  }

  if (readable_bytes < 3) {
    *header_size = 0;
    return true;
  }

  if (error_message != nullptr) {
    *error_message = "three-byte basic header is not implemented yet";
  }
  return false;
}

}  // namespace rmuduo::rtmp
