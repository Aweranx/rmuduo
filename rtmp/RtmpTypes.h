#pragma once

#include <cstdint>
#include <string>

namespace rmuduo::rtmp {

inline constexpr uint8_t kVersion = 3;
inline constexpr uint32_t kDefaultChunkSize = 128;
inline constexpr uint32_t kDefaultAcknowledgementWindow = 5'000'000;
inline constexpr uint32_t kDefaultPeerBandwidth = 5'000'000;

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
