#pragma once

#include <optional>
#include <string>

#include "AuthTypes.h"
#include "TokenManager.h"
#include "UserStore.h"

namespace auth {

class AuthService {
 public:
  AuthService();

  AuthResult login(const std::string& username, const std::string& password,
                   UserRole role);
  std::optional<AuthSession> verifyToken(const std::string& token);
  bool logout(const std::string& token);

 private:
  static std::string makePasswordHash(const std::string& password);

  UserStore userStore_;
  TokenManager tokenManager_;
};

}  // namespace auth
