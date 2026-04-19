#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>

#include "AuthTypes.h"

namespace auth {

class TokenManager {
 public:
  explicit TokenManager(int64_t ttlSeconds = 2 * 60 * 60);

  AuthSession createSession(const User& user);
  std::optional<AuthSession> verify(const std::string& token);
  bool remove(const std::string& token);

 private:
  static int64_t nowSeconds();
  std::string generateToken();
  void removeExpiredLocked(int64_t now);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, AuthSession> sessions_;
  int64_t ttlSeconds_;
  std::random_device randomDevice_;
  std::mt19937_64 generator_;
};

}  // namespace auth
