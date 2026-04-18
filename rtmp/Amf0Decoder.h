#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Amf0Value.h"

namespace rmuduo::rtmp {

class Amf0Decoder {
 public:
  bool decodeAll(std::string_view data, std::vector<Amf0Value>* values,
                 std::string* error_message) const;

 private:
  bool decodeValue(std::string_view data, Amf0Value* value, size_t* bytes_used,
                   std::string* error_message) const;
  bool decodeString(std::string_view data, std::string* value,
                    size_t* bytes_used, std::string* error_message) const;
  bool decodeObject(std::string_view data, Amf0Value::Object* object,
                    size_t* bytes_used, std::string* error_message) const;
};

}  // namespace rmuduo::rtmp
