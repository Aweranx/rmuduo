#include "TokenManager.h"

#include <chrono>
#include <format>

namespace auth {

TokenManager::TokenManager(int64_t ttlSeconds)
    : ttlSeconds_(ttlSeconds), generator_(randomDevice_()) {}

AuthSession TokenManager::createSession(const User& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t now = nowSeconds();
  removeExpiredLocked(now);

  AuthSession session;
  session.token = generateToken();
  session.userId = user.userId;
  session.username = user.username;
  session.role = user.role;
  session.expireAt = now + ttlSeconds_;
  sessions_[session.token] = session;
  return session;
}

std::optional<AuthSession> TokenManager::verify(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t now = nowSeconds();
  auto it = sessions_.find(token);
  if (it == sessions_.end()) {
    return std::nullopt;
  }

  if (it->second.expireAt <= now) {
    sessions_.erase(it);
    return std::nullopt;
  }
  return it->second;
}

bool TokenManager::remove(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  return sessions_.erase(token) > 0;
}

int64_t TokenManager::nowSeconds() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string TokenManager::generateToken() {
  std::string token;
  do {
    token = std::format("{:016x}{:016x}", generator_(), generator_());
  } while (sessions_.contains(token));
  return token;
}

void TokenManager::removeExpiredLocked(int64_t now) {
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (it->second.expireAt <= now) {
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace auth
