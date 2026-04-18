#include "RtmpSession.h"

#include <cstdint>

#include <rmuduo/net/TcpConnection.h>

#include "RtmpChunkWriter.h"
#include "RtmpTypes.h"

namespace rmuduo::rtmp {

namespace {

bool IsAacSequenceHeader(const RtmpMessage& message) {
  if (message.typeId != kMessageTypeAudio || message.payload.size() < 2) {
    return false;
  }
  const uint8_t sound_format =
      (static_cast<uint8_t>(message.payload[0]) >> 4) & 0x0F;
  const uint8_t aac_packet_type = static_cast<uint8_t>(message.payload[1]);
  return sound_format == 10 && aac_packet_type == 0;
}

bool IsAvcSequenceHeader(const RtmpMessage& message) {
  if (message.typeId != kMessageTypeVideo || message.payload.size() < 2) {
    return false;
  }
  const uint8_t codec_id = static_cast<uint8_t>(message.payload[0]) & 0x0F;
  const uint8_t avc_packet_type = static_cast<uint8_t>(message.payload[1]);
  return codec_id == 7 && avc_packet_type == 0;
}

bool IsVideoKeyFrame(const RtmpMessage& message) {
  if (message.typeId != kMessageTypeVideo || message.payload.empty()) {
    return false;
  }
  const uint8_t frame_type =
      (static_cast<uint8_t>(message.payload[0]) >> 4) & 0x0F;
  return frame_type == 1;
}

}  // namespace

RtmpSession::RtmpSession(std::string stream_key)
    : streamKey_(std::move(stream_key)) {}

bool RtmpSession::setPublisher(const TcpConnectionPtr& connection) {
  if (publisher_ && publisher_ != connection) {
    return false;
  }
  if (publisher_ != connection) {
    metadata_.reset();
    audioSequenceHeader_.reset();
    videoSequenceHeader_.reset();
    gopCache_.clear();
    gopStarted_ = false;
  }
  publisher_ = connection;
  return true;
}

void RtmpSession::clearPublisher(const TcpConnectionPtr& connection) {
  if (publisher_ == connection) {
    publisher_.reset();
  }
}

bool RtmpSession::addPlayer(const TcpConnectionPtr& connection) {
  return players_.emplace(connection->name(), connection).second;
}

void RtmpSession::removePlayer(const TcpConnectionPtr& connection) {
  players_.erase(connection->name());
}

void RtmpSession::onMediaMessage(const RtmpMessage& message,
                                 uint32_t out_chunk_size) {
  cacheMediaMessage(message);
  broadcastMessage(message, out_chunk_size);
}

void RtmpSession::replayCachedMessagesToPlayer(
    const TcpConnectionPtr& connection, uint32_t out_chunk_size) const {
  if (!connection) {
    return;
  }

  if (metadata_) {
    connection->send(RtmpChunkWriter::EncodeMessage(*metadata_, out_chunk_size));
  }
  if (audioSequenceHeader_) {
    connection->send(
        RtmpChunkWriter::EncodeMessage(*audioSequenceHeader_, out_chunk_size));
  }
  if (videoSequenceHeader_) {
    connection->send(
        RtmpChunkWriter::EncodeMessage(*videoSequenceHeader_, out_chunk_size));
  }
  for (const auto& message : gopCache_) {
    connection->send(RtmpChunkWriter::EncodeMessage(message, out_chunk_size));
  }
}

void RtmpSession::broadcastMessage(const RtmpMessage& message,
                                   uint32_t out_chunk_size) const {
  if (players_.empty()) {
    return;
  }

  const std::string encoded =
      RtmpChunkWriter::EncodeMessage(message, out_chunk_size);
  for (const auto& [name, player] : players_) {
    (void)name;
    if (player) {
      player->send(encoded);
    }
  }
}

void RtmpSession::cacheMediaMessage(const RtmpMessage& message) {
  if (message.typeId == kMessageTypeDataAmf0) {
    metadata_ = message;
    return;
  }

  if (IsAacSequenceHeader(message)) {
    audioSequenceHeader_ = message;
    return;
  }

  if (IsAvcSequenceHeader(message)) {
    videoSequenceHeader_ = message;
    return;
  }

  if (message.typeId == kMessageTypeVideo && IsVideoKeyFrame(message)) {
    gopCache_.clear();
    gopStarted_ = true;
  }

  if (!gopStarted_) {
    return;
  }

  if (message.typeId == kMessageTypeAudio || message.typeId == kMessageTypeVideo) {
    gopCache_.push_back(message);
  }
}

bool RtmpSession::empty() const { return !publisher_ && players_.empty(); }

}  // namespace rmuduo::rtmp
