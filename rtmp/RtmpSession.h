#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <rmuduo/net/Callbacks.h>

#include "RtmpMessage.h"

namespace rmuduo::rtmp {

class RtmpSession : public std::enable_shared_from_this<RtmpSession> {
 public:
  explicit RtmpSession(std::string stream_key);

  const std::string& streamKey() const { return streamKey_; }

  bool setPublisher(const TcpConnectionPtr& connection);
  void clearPublisher(const TcpConnectionPtr& connection);

  bool addPlayer(const TcpConnectionPtr& connection);
  void removePlayer(const TcpConnectionPtr& connection);
  void onMediaMessage(const RtmpMessage& message, uint32_t out_chunk_size);
  void replayCachedMessagesToPlayer(const TcpConnectionPtr& connection,
                                    uint32_t out_chunk_size) const;

  bool hasPublisher() const { return static_cast<bool>(publisher_); }
  bool empty() const;
  size_t playerCount() const { return players_.size(); }

 private:
  void broadcastMessage(const RtmpMessage& message, uint32_t out_chunk_size) const;
  void cacheMediaMessage(const RtmpMessage& message);

  std::string streamKey_;
  TcpConnectionPtr publisher_;
  std::unordered_map<std::string, TcpConnectionPtr> players_;
  std::optional<RtmpMessage> metadata_;
  std::optional<RtmpMessage> audioSequenceHeader_;
  std::optional<RtmpMessage> videoSequenceHeader_;
  std::vector<RtmpMessage> gopCache_;
  bool gopStarted_ = false;
};

}  // namespace rmuduo::rtmp
