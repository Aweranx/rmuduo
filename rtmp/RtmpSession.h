#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <rmuduo/net/Callbacks.h>

namespace rmuduo::rtmp {

class RtmpSession : public std::enable_shared_from_this<RtmpSession> {
 public:
  explicit RtmpSession(std::string stream_key);

  const std::string& streamKey() const { return streamKey_; }

  bool setPublisher(const TcpConnectionPtr& connection);
  void clearPublisher(const TcpConnectionPtr& connection);

  bool addPlayer(const TcpConnectionPtr& connection);
  void removePlayer(const TcpConnectionPtr& connection);

  bool hasPublisher() const { return static_cast<bool>(publisher_); }
  bool empty() const;
  size_t playerCount() const { return players_.size(); }

 private:
  std::string streamKey_;
  TcpConnectionPtr publisher_;
  std::unordered_map<std::string, TcpConnectionPtr> players_;
};

}  // namespace rmuduo::rtmp
