#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "AuthTypes.h"

namespace auth {

class UserStore {
 public:
  UserStore();

  bool addUser(User user);
  std::optional<User> findByUsername(const std::string& username) const;
  std::optional<User> findByUserId(const std::string& userId) const;

 private:
  static std::string makePasswordHash(const std::string& password);
  void addDefaultUsers();

  mutable std::mutex mutex_;
  std::unordered_map<std::string, User> usersByName_;
  std::unordered_map<std::string, std::string> userIdToName_;
};

}  // namespace auth
