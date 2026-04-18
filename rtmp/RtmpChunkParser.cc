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

  size_t message_header_size = 0;
  if (!parseMessageHeader(data + offset, readable - offset, fmt, &state,
                          &message_header_size, error_message)) {
    return false;
  }
  // fmt=3 在没有 extended timestamp 时，本来就不会额外占用 message header 字节。
  // 这里不能把 0 当成“半包未完成”，否则像 ffmpeg 的 connect 第二个 chunk
  // 就会一直卡在缓冲区里，永远拼不出完整 message。
  const bool fmt3_without_extra_header =
      fmt == kChunkHeaderType3 && state.headerInitialized &&
      state.extendedTimestamp == 0;
  if (message_header_size == 0 && !fmt3_without_extra_header) {
    return true;
  }
  offset += message_header_size;

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

  *csid = 64 + static_cast<uint32_t>(static_cast<uint8_t>(data[1])) +
          (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 8);
  *header_size = 3;
  return true;
}

bool RtmpChunkParser::parseMessageHeader(const char* data, size_t readable_bytes,
                                         uint8_t fmt, ChunkStreamState* state,
                                         size_t* bytes_used,
                                         std::string* error_message) const {
  *bytes_used = 0;

  if (fmt == kChunkHeaderType0) {
    if (readable_bytes < 11) {
      return true;
    }

    const uint32_t timestamp = ReadUint24BE(data);
    state->messageLength = ReadUint24BE(data + 3);
    state->typeId = static_cast<uint8_t>(data[6]);
    state->messageStreamId = ReadUint32LE(data + 7);
    state->timestampDelta = 0;
    *bytes_used = 11;

    if (timestamp == 0x00FFFFFF) {
      if (readable_bytes < *bytes_used + 4) {
        *bytes_used = 0;
        return true;
      }
      state->extendedTimestamp = ReadUint32BE(data + *bytes_used);
      state->timestamp = state->extendedTimestamp;
      *bytes_used += 4;
    } else {
      state->extendedTimestamp = 0;
      state->timestamp = timestamp;
    }

    state->bytesRead = 0;
    state->payload.assign(state->messageLength, '\0');
    state->headerInitialized = true;
    return true;
  }

  if (!state->headerInitialized) {
    if (error_message != nullptr) {
      *error_message = "received continuation chunk before first fmt=0 header";
    }
    return false;
  }

  if (fmt == kChunkHeaderType1) {
    if (readable_bytes < 7) {
      return true;
    }

    const uint32_t timestamp_delta = ReadUint24BE(data);
    state->messageLength = ReadUint24BE(data + 3);
    state->typeId = static_cast<uint8_t>(data[6]);
    *bytes_used = 7;

    if (timestamp_delta == 0x00FFFFFF) {
      if (readable_bytes < *bytes_used + 4) {
        *bytes_used = 0;
        return true;
      }
      state->extendedTimestamp = ReadUint32BE(data + *bytes_used);
      state->timestampDelta = state->extendedTimestamp;
      *bytes_used += 4;
    } else {
      state->extendedTimestamp = 0;
      state->timestampDelta = timestamp_delta;
    }

    state->timestamp += state->timestampDelta;
    state->bytesRead = 0;
    state->payload.assign(state->messageLength, '\0');
    return true;
  }

  if (fmt == kChunkHeaderType2) {
    if (readable_bytes < 3) {
      return true;
    }

    const uint32_t timestamp_delta = ReadUint24BE(data);
    *bytes_used = 3;

    if (timestamp_delta == 0x00FFFFFF) {
      if (readable_bytes < *bytes_used + 4) {
        *bytes_used = 0;
        return true;
      }
      state->extendedTimestamp = ReadUint32BE(data + *bytes_used);
      state->timestampDelta = state->extendedTimestamp;
      *bytes_used += 4;
    } else {
      state->extendedTimestamp = 0;
      state->timestampDelta = timestamp_delta;
    }

    state->timestamp += state->timestampDelta;
    state->bytesRead = 0;
    state->payload.assign(state->messageLength, '\0');
    return true;
  }

  if (fmt == kChunkHeaderType3) {
    // fmt=3 不带新的 message header，沿用上一次该 csid 的头信息。
    // 只有在前面消息未收完时，它才表示“续片”；否则表示一个新消息复用旧头。
    if (state->bytesRead == 0 && state->messageLength > 0) {
      state->timestamp += state->timestampDelta;
      state->payload.assign(state->messageLength, '\0');
    }

    if (state->extendedTimestamp != 0) {
      if (readable_bytes < 4) {
        return true;
      }
      *bytes_used = 4;
    }
    return true;
  }

  if (error_message != nullptr) {
    *error_message = "invalid chunk header fmt";
  }
  return false;
}

}  // namespace rmuduo::rtmp
