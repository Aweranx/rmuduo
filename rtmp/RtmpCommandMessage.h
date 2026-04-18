#pragma once

#include <string>
#include <vector>

#include "Amf0Value.h"
#include "RtmpMessage.h"

namespace rmuduo::rtmp {

struct RtmpCommandMessage {
  std::string name;
  double transactionId = 0.0;
  uint32_t messageStreamId = 0;
  Amf0Value commandObject = Amf0Value::Null();
  std::vector<Amf0Value> arguments;
};

bool DecodeCommandMessageAmf0(const RtmpMessage& message,
                              RtmpCommandMessage* command,
                              std::string* error_message);

}  // namespace rmuduo::rtmp
