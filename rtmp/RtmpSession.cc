#include "RtmpSession.h"

#include <rmuduo/net/TcpConnection.h>

namespace rmuduo::rtmp {

RtmpSession::RtmpSession(std::string stream_key)
    : streamKey_(std::move(stream_key)) {}

bool RtmpSession::setPublisher(const TcpConnectionPtr& connection) {
  if (publisher_ && publisher_ != connection) {
    return false;
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

bool RtmpSession::empty() const { return !publisher_ && players_.empty(); }

}  // namespace rmuduo::rtmp
