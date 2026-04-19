#include "AuthTypes.h"

namespace auth {

std::string_view roleToString(UserRole role) {
  switch (role) {
    case UserRole::kTeacher:
      return "teacher";
    case UserRole::kStudent:
      return "student";
    case UserRole::kUnknown:
      return "unknown";
  }
  return "unknown";
}

UserRole roleFromString(std::string_view role) {
  if (role == "teacher") {
    return UserRole::kTeacher;
  }
  if (role == "student") {
    return UserRole::kStudent;
  }
  return UserRole::kUnknown;
}

std::string_view authErrorMessage(AuthError error) {
  switch (error) {
    case AuthError::kOk:
      return "ok";
    case AuthError::kBadRequest:
      return "bad request";
    case AuthError::kInvalidCredentials:
      return "invalid username or password";
    case AuthError::kUnauthorized:
      return "unauthorized";
    case AuthError::kTokenExpired:
      return "token expired";
    case AuthError::kUserDisabled:
      return "user disabled";
    case AuthError::kUnsupportedRole:
      return "unsupported role";
  }
  return "internal error";
}

}  // namespace auth
