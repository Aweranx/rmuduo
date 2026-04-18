#include "RtmpCommandMessage.h"

#include <utility>
#include <vector>

#include "Amf0Decoder.h"
#include "RtmpTypes.h"

namespace rmuduo::rtmp {

bool DecodeCommandMessageAmf0(const RtmpMessage& message,
                              RtmpCommandMessage* command,
                              std::string* error_message) {
  if (message.typeId != kMessageTypeCommandAmf0) {
    if (error_message != nullptr) {
      *error_message = "message is not command message amf0";
    }
    return false;
  }

  std::vector<Amf0Value> values;
  Amf0Decoder decoder;
  if (!decoder.decodeAll(message.payload, &values, error_message)) {
    return false;
  }

  if (values.size() < 2) {
    if (error_message != nullptr) {
      *error_message = "command message must contain at least name and transaction id";
    }
    return false;
  }

  if (!values[0].isString() || !values[1].isNumber()) {
    if (error_message != nullptr) {
      *error_message = "command message prefix types are invalid";
    }
    return false;
  }

  command->name = values[0].stringValue();
  command->transactionId = values[1].numberValue();
  command->messageStreamId = message.messageStreamId;
  command->commandObject =
      values.size() >= 3 ? values[2] : Amf0Value::Null();
  command->arguments.assign(values.begin() + std::min<size_t>(3, values.size()),
                            values.end());
  return true;
}

}  // namespace rmuduo::rtmp
