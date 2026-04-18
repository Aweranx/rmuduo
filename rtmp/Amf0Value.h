#pragma once

#include <map>
#include <string>
#include <vector>

namespace rmuduo::rtmp {

enum class Amf0Type {
  kNumber,
  kBoolean,
  kString,
  kObject,
  kNull,
  kEcmaArray,
};

class Amf0Value {
 public:
  using Object = std::map<std::string, Amf0Value>;

  static Amf0Value Number(double value);
  static Amf0Value Boolean(bool value);
  static Amf0Value String(std::string value);
  static Amf0Value ObjectValue(Object value);
  static Amf0Value EcmaArray(Object value);
  static Amf0Value Null();

  Amf0Type type() const { return type_; }
  double numberValue() const { return numberValue_; }
  bool booleanValue() const { return booleanValue_; }
  const std::string& stringValue() const { return stringValue_; }
  const Object& objectValue() const { return objectValue_; }

  bool isString() const { return type_ == Amf0Type::kString; }
  bool isNumber() const { return type_ == Amf0Type::kNumber; }
  bool isObjectLike() const {
    return type_ == Amf0Type::kObject || type_ == Amf0Type::kEcmaArray;
  }

  std::string typeName() const;

 private:
  explicit Amf0Value(Amf0Type type) : type_(type) {}

  Amf0Type type_ = Amf0Type::kNull;
  double numberValue_ = 0.0;
  bool booleanValue_ = false;
  std::string stringValue_;
  Object objectValue_;
};

}  // namespace rmuduo::rtmp
