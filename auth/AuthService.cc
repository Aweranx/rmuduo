#include "AuthService.h"

#include <functional>

namespace auth {

AuthService::AuthService() = default;

AuthResult AuthService::login(const std::string& username,
                              const std::string& password, UserRole role) {
  if (username.empty() || password.empty() || role == UserRole::kUnknown) {
    return AuthResult{.error = AuthError::kBadRequest};
  }

  auto user = userStore_.findByUsername(username);
  if (!user || user->passwordHash != makePasswordHash(password)) {
    return AuthResult{.error = AuthError::kInvalidCredentials};
  }

  if (!user->enabled) {
    return AuthResult{.error = AuthError::kUserDisabled};
  }

  if (user->role != role) {
    return AuthResult{.error = AuthError::kUnsupportedRole};
  }

  return AuthResult{.error = AuthError::kOk,
                    .session = tokenManager_.createSession(*user)};
}

std::optional<AuthSession> AuthService::verifyToken(const std::string& token) {
  if (token.empty()) {
    return std::nullopt;
  }
  return tokenManager_.verify(token);
}

bool AuthService::logout(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  return tokenManager_.remove(token);
}

std::string AuthService::makePasswordHash(const std::string& password) {
  return std::to_string(std::hash<std::string>{}(password));
}

}  // namespace auth
