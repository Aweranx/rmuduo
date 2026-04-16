#include "RtmpSessionManager.h"

namespace rmuduo::rtmp {

std::shared_ptr<RtmpSession> RtmpSessionManager::getOrCreate(
    const std::string& stream_key) {
  auto it = sessions_.find(stream_key);
  if (it != sessions_.end()) {
    return it->second;
  }

  auto session = std::make_shared<RtmpSession>(stream_key);
  sessions_.emplace(stream_key, session);
  return session;
}

std::shared_ptr<RtmpSession> RtmpSessionManager::find(
    const std::string& stream_key) const {
  auto it = sessions_.find(stream_key);
  if (it == sessions_.end()) {
    return nullptr;
  }
  return it->second;
}

void RtmpSessionManager::removeIfEmpty(const std::string& stream_key) {
  auto it = sessions_.find(stream_key);
  if (it != sessions_.end() && it->second->empty()) {
    sessions_.erase(it);
  }
}

}  // namespace rmuduo::rtmp
