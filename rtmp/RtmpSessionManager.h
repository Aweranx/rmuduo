#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "RtmpSession.h"

namespace rmuduo::rtmp {

class RtmpSessionManager {
 public:
  std::shared_ptr<RtmpSession> getOrCreate(const std::string& stream_key);
  std::shared_ptr<RtmpSession> find(const std::string& stream_key) const;
  void removeIfEmpty(const std::string& stream_key);
  size_t size() const { return sessions_.size(); }

 private:
  std::unordered_map<std::string, std::shared_ptr<RtmpSession>> sessions_;
};

}  // namespace rmuduo::rtmp
