#include "UserStore.h"

#include <functional>

namespace auth {

UserStore::UserStore() {
  addDefaultUsers();
}

bool UserStore::addUser(User user) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (usersByName_.contains(user.username) ||
      userIdToName_.contains(user.userId)) {
    return false;
  }

  userIdToName_[user.userId] = user.username;
  usersByName_[user.username] = std::move(user);
  return true;
}

std::optional<User> UserStore::findByUsername(
    const std::string& username) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = usersByName_.find(username);
  if (it == usersByName_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<User> UserStore::findByUserId(const std::string& userId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto idIt = userIdToName_.find(userId);
  if (idIt == userIdToName_.end()) {
    return std::nullopt;
  }

  auto userIt = usersByName_.find(idIt->second);
  if (userIt == usersByName_.end()) {
    return std::nullopt;
  }
  return userIt->second;
}

std::string UserStore::makePasswordHash(const std::string& password) {
  return std::to_string(std::hash<std::string>{}(password));
}

void UserStore::addDefaultUsers() {
  addUser(User{.userId = "teacher01",
               .username = "teacher01",
               .passwordHash = makePasswordHash("123456"),
               .role = UserRole::kTeacher,
               .enabled = true});
  addUser(User{.userId = "student01",
               .username = "student01",
               .passwordHash = makePasswordHash("123456"),
               .role = UserRole::kStudent,
               .enabled = true});
  addUser(User{.userId = "student02",
               .username = "student02",
               .passwordHash = makePasswordHash("123456"),
               .role = UserRole::kStudent,
               .enabled = true});
}

}  // namespace auth
