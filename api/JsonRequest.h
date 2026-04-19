#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace api {

class JsonObject {
 public:
  bool contains(std::string_view key) const;
  std::optional<std::string> getString(std::string_view key) const;

 private:
  friend std::optional<JsonObject> parseJsonObject(std::string_view input);

  std::unordered_map<std::string, std::string> strings_;
};

std::optional<JsonObject> parseJsonObject(std::string_view input);

}  // namespace api
