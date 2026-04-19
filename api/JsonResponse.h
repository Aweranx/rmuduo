#pragma once

#include <string>
#include <string_view>

namespace api {

inline std::string escapeJson(std::string_view value) {
  std::string result;
  result.reserve(value.size() + 8);
  for (char ch : value) {
    switch (ch) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\b':
        result += "\\b";
        break;
      case '\f':
        result += "\\f";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += ch;
        break;
    }
  }
  return result;
}

inline std::string quote(std::string_view value) {
  return "\"" + escapeJson(value) + "\"";
}

inline std::string makeJsonResponse(int code, std::string_view message,
                                    std::string_view dataJson = "null") {
  return "{\"code\":" + std::to_string(code) + ",\"message\":" +
         quote(message) + ",\"data\":" + std::string(dataJson) + "}";
}

inline std::string makeOk(std::string_view dataJson = "null") {
  return makeJsonResponse(0, "ok", dataJson);
}

inline std::string makeError(int code, std::string_view message) {
  return makeJsonResponse(code, message, "null");
}

inline std::string makeField(std::string_view key, std::string_view value) {
  return quote(key) + ":" + quote(value);
}

inline std::string makeNumberField(std::string_view key, int64_t value) {
  return quote(key) + ":" + std::to_string(value);
}

}  // namespace api
