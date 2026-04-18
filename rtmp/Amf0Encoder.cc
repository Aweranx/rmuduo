#include "Amf0Encoder.h"

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

}  // namespace

void Amf0Encoder::appendValue(const Amf0Value& value) {
  switch (value.type()) {
    case Amf0Type::kNumber:
      appendNumber(value.numberValue());
      return;
    case Amf0Type::kBoolean:
      appendBoolean(value.booleanValue());
      return;
    case Amf0Type::kString:
      appendString(value.stringValue());
      return;
    case Amf0Type::kObject:
      appendObject(value.objectValue(), false);
      return;
    case Amf0Type::kEcmaArray:
      appendObject(value.objectValue(), true);
      return;
    case Amf0Type::kNull:
      appendNull();
      return;
  }
}

void Amf0Encoder::appendString(const std::string& value) {
  data_.push_back(static_cast<char>(kAmf0String));
  appendUint16BE(static_cast<uint16_t>(value.size()));
  data_.append(value);
}

void Amf0Encoder::appendNumber(double value) {
  data_.push_back(static_cast<char>(kAmf0Number));
  appendDoubleBE(value);
}

void Amf0Encoder::appendBoolean(bool value) {
  data_.push_back(static_cast<char>(kAmf0Boolean));
  data_.push_back(value ? '\x01' : '\x00');
}

void Amf0Encoder::appendNull() { data_.push_back(static_cast<char>(kAmf0Null)); }

void Amf0Encoder::appendObject(const Amf0Value::Object& object,
                               bool ecma_array) {
  data_.push_back(static_cast<char>(ecma_array ? kAmf0EcmaArray : kAmf0Object));
  if (ecma_array) {
    appendUint32BE(static_cast<uint32_t>(object.size()));
  }

  for (const auto& [key, value] : object) {
    appendObjectProperty(key, value);
  }

  appendUint16BE(0);
  data_.push_back(static_cast<char>(kAmf0ObjectEnd));
}

void Amf0Encoder::appendUint16BE(uint16_t value) {
  data_.push_back(static_cast<char>((value >> 8) & 0xFF));
  data_.push_back(static_cast<char>(value & 0xFF));
}

void Amf0Encoder::appendUint32BE(uint32_t value) {
  data_.push_back(static_cast<char>((value >> 24) & 0xFF));
  data_.push_back(static_cast<char>((value >> 16) & 0xFF));
  data_.push_back(static_cast<char>((value >> 8) & 0xFF));
  data_.push_back(static_cast<char>(value & 0xFF));
}

void Amf0Encoder::appendDoubleBE(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  data_.push_back(static_cast<char>((bits >> 56) & 0xFF));
  data_.push_back(static_cast<char>((bits >> 48) & 0xFF));
  data_.push_back(static_cast<char>((bits >> 40) & 0xFF));
  data_.push_back(static_cast<char>((bits >> 32) & 0xFF));
  data_.push_back(static_cast<char>((bits >> 24) & 0xFF));
  data_.push_back(static_cast<char>((bits >> 16) & 0xFF));
  data_.push_back(static_cast<char>((bits >> 8) & 0xFF));
  data_.push_back(static_cast<char>(bits & 0xFF));
}

void Amf0Encoder::appendObjectProperty(const std::string& key,
                                       const Amf0Value& value) {
  appendUint16BE(static_cast<uint16_t>(key.size()));
  data_.append(key);
  appendValue(value);
}

}  // namespace rmuduo::rtmp
