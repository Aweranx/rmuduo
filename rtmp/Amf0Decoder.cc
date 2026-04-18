#include "Amf0Decoder.h"

#include <cstdint>
#include <cstring>

namespace rmuduo::rtmp {

namespace {

constexpr uint8_t kAmf0Number = 0x00;
constexpr uint8_t kAmf0Boolean = 0x01;
constexpr uint8_t kAmf0String = 0x02;
constexpr uint8_t kAmf0Object = 0x03;
constexpr uint8_t kAmf0Null = 0x05;
constexpr uint8_t kAmf0EcmaArray = 0x08;
constexpr uint8_t kAmf0ObjectEnd = 0x09;

uint16_t ReadUint16BE(const char* data) {
  return (static_cast<uint16_t>(static_cast<uint8_t>(data[0])) << 8) |
         static_cast<uint16_t>(static_cast<uint8_t>(data[1]));
}

uint32_t ReadUint32BE(const char* data) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(data[0])) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 8) |
         static_cast<uint32_t>(static_cast<uint8_t>(data[3]));
}

double ReadDoubleBE(const char* data) {
  uint64_t bits = (static_cast<uint64_t>(static_cast<uint8_t>(data[0])) << 56) |
                  (static_cast<uint64_t>(static_cast<uint8_t>(data[1])) << 48) |
                  (static_cast<uint64_t>(static_cast<uint8_t>(data[2])) << 40) |
                  (static_cast<uint64_t>(static_cast<uint8_t>(data[3])) << 32) |
                  (static_cast<uint64_t>(static_cast<uint8_t>(data[4])) << 24) |
                  (static_cast<uint64_t>(static_cast<uint8_t>(data[5])) << 16) |
                  (static_cast<uint64_t>(static_cast<uint8_t>(data[6])) << 8) |
                  static_cast<uint64_t>(static_cast<uint8_t>(data[7]));
  double value = 0.0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

}  // namespace

bool Amf0Decoder::decodeAll(std::string_view data,
                            std::vector<Amf0Value>* values,
                            std::string* error_message) const {
  size_t offset = 0;
  values->clear();

  while (offset < data.size()) {
    Amf0Value value = Amf0Value::Null();
    size_t bytes_used = 0;
    if (!decodeValue(data.substr(offset), &value, &bytes_used, error_message)) {
      return false;
    }
    if (bytes_used == 0) {
      break;
    }

    values->push_back(std::move(value));
    offset += bytes_used;
  }

  return true;
}

bool Amf0Decoder::decodeValue(std::string_view data, Amf0Value* value,
                              size_t* bytes_used,
                              std::string* error_message) const {
  *bytes_used = 0;
  if (data.empty()) {
    return true;
  }

  const uint8_t type = static_cast<uint8_t>(data[0]);
  if (type == kAmf0Number) {
    if (data.size() < 1 + 8) {
      if (error_message != nullptr) {
        *error_message = "amf0 number payload is truncated";
      }
      return false;
    }
    *value = Amf0Value::Number(ReadDoubleBE(data.data() + 1));
    *bytes_used = 9;
    return true;
  }

  if (type == kAmf0Boolean) {
    if (data.size() < 2) {
      if (error_message != nullptr) {
        *error_message = "amf0 boolean payload is truncated";
      }
      return false;
    }
    *value = Amf0Value::Boolean(data[1] != '\0');
    *bytes_used = 2;
    return true;
  }

  if (type == kAmf0String) {
    std::string string_value;
    size_t string_bytes = 0;
    if (!decodeString(data.substr(1), &string_value, &string_bytes,
                      error_message)) {
      return false;
    }
    *value = Amf0Value::String(std::move(string_value));
    *bytes_used = 1 + string_bytes;
    return true;
  }

  if (type == kAmf0Object) {
    Amf0Value::Object object_value;
    size_t object_bytes = 0;
    if (!decodeObject(data.substr(1), &object_value, &object_bytes,
                      error_message)) {
      return false;
    }
    *value = Amf0Value::ObjectValue(std::move(object_value));
    *bytes_used = 1 + object_bytes;
    return true;
  }

  if (type == kAmf0EcmaArray) {
    if (data.size() < 5) {
      if (error_message != nullptr) {
        *error_message = "amf0 ecma array payload is truncated";
      }
      return false;
    }

    (void)ReadUint32BE(data.data() + 1);
    Amf0Value::Object object_value;
    size_t object_bytes = 0;
    if (!decodeObject(data.substr(5), &object_value, &object_bytes,
                      error_message)) {
      return false;
    }
    *value = Amf0Value::EcmaArray(std::move(object_value));
    *bytes_used = 5 + object_bytes;
    return true;
  }

  if (type == kAmf0Null) {
    *value = Amf0Value::Null();
    *bytes_used = 1;
    return true;
  }

  if (error_message != nullptr) {
    *error_message = "unsupported amf0 type marker";
  }
  return false;
}

bool Amf0Decoder::decodeString(std::string_view data, std::string* value,
                               size_t* bytes_used,
                               std::string* error_message) const {
  *bytes_used = 0;
  if (data.size() < 2) {
    if (error_message != nullptr) {
      *error_message = "amf0 string length is truncated";
    }
    return false;
  }

  const size_t length = ReadUint16BE(data.data());
  if (data.size() < 2 + length) {
    if (error_message != nullptr) {
      *error_message = "amf0 string payload is truncated";
    }
    return false;
  }

  *value = std::string(data.substr(2, length));
  *bytes_used = 2 + length;
  return true;
}

bool Amf0Decoder::decodeObject(std::string_view data, Amf0Value::Object* object,
                               size_t* bytes_used,
                               std::string* error_message) const {
  *bytes_used = 0;
  object->clear();

  size_t offset = 0;
  while (offset < data.size()) {
    if (data.size() - offset >= 3 && data[offset] == '\0' &&
        data[offset + 1] == '\0' &&
        static_cast<uint8_t>(data[offset + 2]) == kAmf0ObjectEnd) {
      *bytes_used = offset + 3;
      return true;
    }

    std::string key;
    size_t key_bytes = 0;
    if (!decodeString(data.substr(offset), &key, &key_bytes, error_message)) {
      return false;
    }
    offset += key_bytes;

    Amf0Value property = Amf0Value::Null();
    size_t value_bytes = 0;
    if (!decodeValue(data.substr(offset), &property, &value_bytes,
                     error_message)) {
      return false;
    }
    if (value_bytes == 0) {
      if (error_message != nullptr) {
        *error_message = "amf0 object property is empty";
      }
      return false;
    }

    object->emplace(std::move(key), std::move(property));
    offset += value_bytes;
  }

  if (error_message != nullptr) {
    *error_message = "amf0 object end marker is missing";
  }
  return false;
}

}  // namespace rmuduo::rtmp
