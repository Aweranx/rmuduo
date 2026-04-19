#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace auth {

enum class UserRole {
  kUnknown,
  kTeacher,
  kStudent,
};

struct User {
  std::string userId;
  std::string username;
  std::string passwordHash;
  UserRole role = UserRole::kUnknown;
  bool enabled = true;
};

struct AuthSession {
  std::string token;
  std::string userId;
  std::string username;
  UserRole role = UserRole::kUnknown;
  int64_t expireAt = 0;
};

enum class AuthError {
  kOk,
  kBadRequest,
  kInvalidCredentials,
  kUnauthorized,
  kTokenExpired,
  kUserDisabled,
  kUnsupportedRole,
};

struct AuthResult {
  AuthError error = AuthError::kOk;
  AuthSession session;
};

std::string_view roleToString(UserRole role);
UserRole roleFromString(std::string_view role);
std::string_view authErrorMessage(AuthError error);

}  // namespace auth
