#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>

#include "AuthTypes.h"

namespace auth {

class RtmpTicketManager {
 public:
  explicit RtmpTicketManager(int64_t ttlSeconds = 5 * 60);

  RtmpTicket createTicket(const AuthSession& session,
                          const std::string& streamKey,
                          RtmpPermission permission);
  std::optional<RtmpTicket> verifyPublish(const std::string& ticket,
                                          const std::string& streamKey);
  std::optional<RtmpTicket> verifyPlay(const std::string& ticket,
                                       const std::string& streamKey);

 private:
  static int64_t nowSeconds();
  std::string generateTicket();
  std::optional<RtmpTicket> verify(const std::string& ticket,
                                   const std::string& streamKey,
                                   bool requirePublish);
  void removeExpiredLocked(int64_t now);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, RtmpTicket> tickets_;
  int64_t ttlSeconds_;
  std::random_device randomDevice_;
  std::mt19937_64 generator_;
};

}  // namespace auth
