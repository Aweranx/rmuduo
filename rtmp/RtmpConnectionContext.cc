#include "RtmpConnectionContext.h"

#include "RtmpSession.h"

namespace rmuduo::rtmp {

std::string RtmpConnectionContext::streamKey() const {
  if (app_.empty() || streamName_.empty()) {
    return {};
  }
  return MakeStreamKey(app_, streamName_);
}

void RtmpConnectionContext::bindSession(
    const std::shared_ptr<RtmpSession>& session) {
  session_ = session;
}

std::shared_ptr<RtmpSession> RtmpConnectionContext::session() const {
  return session_.lock();
}

void RtmpConnectionContext::clearSession() { session_.reset(); }

}  // namespace rmuduo::rtmp
