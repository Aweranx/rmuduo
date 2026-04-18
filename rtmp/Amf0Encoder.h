#pragma once

#include <cstdint>
#include <string>

#include "Amf0Value.h"

namespace rmuduo::rtmp {

class Amf0Encoder {
 public:
  void appendValue(const Amf0Value& value);
  void appendString(const std::string& value);
  void appendNumber(double value);
  void appendBoolean(bool value);
  void appendNull();
  void appendObject(const Amf0Value::Object& object, bool ecma_array = false);

  const std::string& data() const { return data_; }
  std::string takeData() { return std::move(data_); }

 private:
  void appendUint16BE(uint16_t value);
  void appendUint32BE(uint32_t value);
  void appendDoubleBE(double value);
  void appendObjectProperty(const std::string& key, const Amf0Value& value);

  std::string data_;
};

}  // namespace rmuduo::rtmp
