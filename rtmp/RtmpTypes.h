#pragma once

#include <cstdint>
#include <string>

namespace rmuduo::rtmp {

inline constexpr uint8_t kVersion = 3;
inline constexpr uint32_t kDefaultChunkSize = 128;
inline constexpr uint32_t kDefaultAcknowledgementWindow = 5'000'000;
inline constexpr uint32_t kDefaultPeerBandwidth = 5'000'000;

inline constexpr uint8_t kChunkHeaderType0 = 0;
inline constexpr uint8_t kChunkHeaderType1 = 1;
inline constexpr uint8_t kChunkHeaderType2 = 2;
inline constexpr uint8_t kChunkHeaderType3 = 3;

inline constexpr uint8_t kMessageTypeSetChunkSize = 1;
inline constexpr uint8_t kMessageTypeAbort = 2;
inline constexpr uint8_t kMessageTypeAcknowledgement = 3;
inline constexpr uint8_t kMessageTypeUserControl = 4;
inline constexpr uint8_t kMessageTypeWindowAcknowledgementSize = 5;
inline constexpr uint8_t kMessageTypeSetPeerBandwidth = 6;
inline constexpr uint8_t kMessageTypeAudio = 8;
inline constexpr uint8_t kMessageTypeVideo = 9;
inline constexpr uint8_t kMessageTypeDataAmf3 = 15;
inline constexpr uint8_t kMessageTypeSharedObjectAmf3 = 16;
inline constexpr uint8_t kMessageTypeCommandAmf3 = 17;
inline constexpr uint8_t kMessageTypeDataAmf0 = 18;
inline constexpr uint8_t kMessageTypeSharedObjectAmf0 = 19;
inline constexpr uint8_t kMessageTypeCommandAmf0 = 20;

enum class HandshakeState : uint8_t {
  kWaitC0C1,
  kSendS0S1S2,
  kWaitC2,
  kHandshakeDone,
};

enum class ConnectionRole : uint8_t {
  kUnknown,
  kPublisher,
  kPlayer,
};

enum class SessionAttachResult : uint8_t {
  kAttached,
  kRejected,
};

inline std::string MakeStreamKey(const std::string& app,
                                 const std::string& stream_name) {
  return "/" + app + "/" + stream_name;
}

}  // namespace rmuduo::rtmp
