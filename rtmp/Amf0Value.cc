#include "Amf0Value.h"

namespace rmuduo::rtmp {

Amf0Value Amf0Value::Number(double value) {
  Amf0Value result(Amf0Type::kNumber);
  result.numberValue_ = value;
  return result;
}

Amf0Value Amf0Value::Boolean(bool value) {
  Amf0Value result(Amf0Type::kBoolean);
  result.booleanValue_ = value;
  return result;
}

Amf0Value Amf0Value::String(std::string value) {
  Amf0Value result(Amf0Type::kString);
  result.stringValue_ = std::move(value);
  return result;
}

Amf0Value Amf0Value::ObjectValue(Object value) {
  Amf0Value result(Amf0Type::kObject);
  result.objectValue_ = std::move(value);
  return result;
}

Amf0Value Amf0Value::EcmaArray(Object value) {
  Amf0Value result(Amf0Type::kEcmaArray);
  result.objectValue_ = std::move(value);
  return result;
}

Amf0Value Amf0Value::Null() { return Amf0Value(Amf0Type::kNull); }

std::string Amf0Value::typeName() const {
  switch (type_) {
    case Amf0Type::kNumber:
      return "number";
    case Amf0Type::kBoolean:
      return "boolean";
    case Amf0Type::kString:
      return "string";
    case Amf0Type::kObject:
      return "object";
    case Amf0Type::kNull:
      return "null";
    case Amf0Type::kEcmaArray:
      return "ecma-array";
  }
  return "unknown";
}

}  // namespace rmuduo::rtmp
