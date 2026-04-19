#include "RtmpTicketManager.h"

#include <chrono>
#include <format>

namespace auth {

RtmpTicketManager::RtmpTicketManager(int64_t ttlSeconds)
    : ttlSeconds_(ttlSeconds), generator_(randomDevice_()) {}

RtmpTicket RtmpTicketManager::createTicket(const AuthSession& session,
                                           const std::string& streamKey,
                                           RtmpPermission permission) {
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t now = nowSeconds();
  removeExpiredLocked(now);

  RtmpTicket ticket;
  ticket.ticket = generateTicket();
  ticket.userId = session.userId;
  ticket.username = session.username;
  ticket.role = session.role;
  ticket.streamKey = streamKey;
  ticket.allowPublish = permission == RtmpPermission::kPublish ||
                        permission == RtmpPermission::kBoth;
  ticket.allowPlay =
      permission == RtmpPermission::kPlay || permission == RtmpPermission::kBoth;
  ticket.expireAt = now + ttlSeconds_;
  tickets_[ticket.ticket] = ticket;
  return ticket;
}

std::optional<RtmpTicket> RtmpTicketManager::verifyPublish(
    const std::string& ticket, const std::string& streamKey) {
  return verify(ticket, streamKey, true);
}

std::optional<RtmpTicket> RtmpTicketManager::verifyPlay(
    const std::string& ticket, const std::string& streamKey) {
  return verify(ticket, streamKey, false);
}

int64_t RtmpTicketManager::nowSeconds() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string RtmpTicketManager::generateTicket() {
  std::string ticket;
  do {
    ticket = std::format("{:016x}{:016x}", generator_(), generator_());
  } while (tickets_.contains(ticket));
  return ticket;
}

std::optional<RtmpTicket> RtmpTicketManager::verify(
    const std::string& ticket, const std::string& streamKey,
    bool requirePublish) {
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t now = nowSeconds();
  auto it = tickets_.find(ticket);
  if (it == tickets_.end()) {
    return std::nullopt;
  }

  if (it->second.expireAt <= now) {
    tickets_.erase(it);
    return std::nullopt;
  }

  if (it->second.streamKey != streamKey) {
    return std::nullopt;
  }

  if (requirePublish && !it->second.allowPublish) {
    return std::nullopt;
  }
  if (!requirePublish && !it->second.allowPlay) {
    return std::nullopt;
  }

  return it->second;
}

void RtmpTicketManager::removeExpiredLocked(int64_t now) {
  for (auto it = tickets_.begin(); it != tickets_.end();) {
    if (it->second.expireAt <= now) {
      it = tickets_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace auth
