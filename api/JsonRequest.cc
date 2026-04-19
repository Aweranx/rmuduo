#include "JsonRequest.h"

#include <cctype>

namespace api {
namespace {

void skipSpaces(std::string_view input, size_t* pos) {
  while (*pos < input.size() &&
         std::isspace(static_cast<unsigned char>(input[*pos]))) {
    ++(*pos);
  }
}

bool parseString(std::string_view input, size_t* pos, std::string* output) {
  if (*pos >= input.size() || input[*pos] != '"') {
    return false;
  }
  ++(*pos);

  output->clear();
  while (*pos < input.size()) {
    char ch = input[*pos];
    ++(*pos);

    if (ch == '"') {
      return true;
    }

    if (ch != '\\') {
      output->push_back(ch);
      continue;
    }

    if (*pos >= input.size()) {
      return false;
    }

    char escaped = input[*pos];
    ++(*pos);
    switch (escaped) {
      case '"':
      case '\\':
      case '/':
        output->push_back(escaped);
        break;
      case 'b':
        output->push_back('\b');
        break;
      case 'f':
        output->push_back('\f');
        break;
      case 'n':
        output->push_back('\n');
        break;
      case 'r':
        output->push_back('\r');
        break;
      case 't':
        output->push_back('\t');
        break;
      default:
        return false;
    }
  }
  return false;
}

}  // namespace

bool JsonObject::contains(std::string_view key) const {
  return strings_.contains(std::string(key));
}

std::optional<std::string> JsonObject::getString(std::string_view key) const {
  auto it = strings_.find(std::string(key));
  if (it == strings_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<JsonObject> parseJsonObject(std::string_view input) {
  JsonObject object;
  size_t pos = 0;

  skipSpaces(input, &pos);
  if (pos >= input.size() || input[pos] != '{') {
    return std::nullopt;
  }
  ++pos;

  skipSpaces(input, &pos);
  if (pos < input.size() && input[pos] == '}') {
    ++pos;
    skipSpaces(input, &pos);
    return pos == input.size() ? std::optional<JsonObject>(std::move(object))
                               : std::nullopt;
  }

  while (pos < input.size()) {
    std::string key;
    std::string value;

    skipSpaces(input, &pos);
    if (!parseString(input, &pos, &key)) {
      return std::nullopt;
    }

    skipSpaces(input, &pos);
    if (pos >= input.size() || input[pos] != ':') {
      return std::nullopt;
    }
    ++pos;

    skipSpaces(input, &pos);
    if (!parseString(input, &pos, &value)) {
      return std::nullopt;
    }
    object.strings_[std::move(key)] = std::move(value);

    skipSpaces(input, &pos);
    if (pos >= input.size()) {
      return std::nullopt;
    }
    if (input[pos] == '}') {
      ++pos;
      skipSpaces(input, &pos);
      return pos == input.size() ? std::optional<JsonObject>(std::move(object))
                                 : std::nullopt;
    }
    if (input[pos] != ',') {
      return std::nullopt;
    }
    ++pos;
  }

  return std::nullopt;
}

}  // namespace api
