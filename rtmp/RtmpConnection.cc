#include "RtmpConnection.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <rmuduo/net/Buffer.h>
#include <rmuduo/net/Logger.h>
#include <rmuduo/net/TcpConnection.h>

#include "Amf0Encoder.h"
#include "RtmpChunkWriter.h"
#include "RtmpConnectionContext.h"

namespace rmuduo::rtmp {

namespace {

uint32_t ReadUint32BE(const char* data) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(data[0])) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 8) |
         static_cast<uint32_t>(static_cast<uint8_t>(data[3]));
}

void AppendUint32BE(std::string* out, uint32_t value) {
  out->push_back(static_cast<char>((value >> 24) & 0xFF));
  out->push_back(static_cast<char>((value >> 16) & 0xFF));
  out->push_back(static_cast<char>((value >> 8) & 0xFF));
  out->push_back(static_cast<char>(value & 0xFF));
}

}  // namespace

bool RtmpConnection::onMessage(const TcpConnectionPtr& conn, Buffer* buf,
                               Timestamp receiveTime,
                               RtmpConnectionContext* context) {
  (void)receiveTime;
  if (context == nullptr) {
    LOG_ERROR("RtmpConnection missing connection context for {}", conn->name());
    return false;
  }

  context->addReceivedBytes(buf->readableBytes());

  if (!context->handshake().isDone()) {
    std::string response;
    std::string error_message;
    if (!context->handshake().process(buf, &response, &error_message)) {
      LOG_ERROR("RtmpConnection handshake failed on {}: {}", conn->name(),
                error_message);
      return false;
    }

    if (!response.empty()) {
      conn->send(response);
      LOG_INFO("RtmpConnection sent {} handshake bytes on {}", response.size(),
               conn->name());
    }

    if (!context->handshake().isDone()) {
      return true;
    }

    LOG_INFO("RtmpConnection handshake completed on {}", conn->name());
  }

  if (buf->readableBytes() == 0) {
    return true;
  }

  std::vector<RtmpMessage> messages;
  std::string error_message;
  if (!context->chunkParser().parse(buf, &messages, &error_message)) {
    LOG_ERROR("RtmpConnection chunk parse failed on {}: {}", conn->name(),
              error_message);
    return false;
  }

  for (const auto& message : messages) {
    if (!handleMessage(conn, message, context)) {
      return false;
    }
  }

  return true;
}

bool RtmpConnection::handleMessage(const TcpConnectionPtr& conn,
                                   const RtmpMessage& message,
                                   RtmpConnectionContext* context) {
  LOG_INFO(
      "RtmpConnection parsed message on {}: csid={}, type={}, len={}, stream={}",
      conn->name(), message.chunkStreamId, message.typeId,
      message.messageLength, message.messageStreamId);

  if (message.typeId == kMessageTypeSetChunkSize &&
      message.payload.size() >= sizeof(uint32_t)) {
    const uint32_t chunk_size = ReadUint32BE(message.payload.data());
    context->setInChunkSize(chunk_size);
    context->chunkParser().setInChunkSize(chunk_size);
    LOG_INFO("RtmpConnection updated inbound chunk size on {} to {}",
             conn->name(), chunk_size);
    return true;
  }

  if (message.typeId == kMessageTypeCommandAmf0) {
    RtmpCommandMessage command;
    std::string command_error;
    if (!DecodeCommandMessageAmf0(message, &command, &command_error)) {
      LOG_ERROR("RtmpConnection failed to decode AMF0 command on {}: {}",
                conn->name(), command_error);
      return false;
    }

    LOG_INFO(
        "RtmpConnection parsed AMF0 command on {}: name={}, transactionId={}, args={}",
        conn->name(), command.name, command.transactionId,
        command.arguments.size());

    return handleCommandMessage(conn, context, command);
  }

  return true;
}

bool RtmpConnection::handleCommandMessage(const TcpConnectionPtr& conn,
                                          RtmpConnectionContext* context,
                                          const RtmpCommandMessage& command) {
  if (command.name == "connect") {
    return handleConnect(conn, context, command);
  }

  if (command.name == "createStream") {
    return handleCreateStream(conn, context, command);
  }

  LOG_INFO("RtmpConnection ignores command {} on {}", command.name,
           conn->name());
  return true;
}

bool RtmpConnection::handleConnect(const TcpConnectionPtr& conn,
                                   RtmpConnectionContext* context,
                                   const RtmpCommandMessage& command) {
  if (!command.commandObject.isObjectLike()) {
    LOG_ERROR("RtmpConnection connect command object is not an object on {}",
              conn->name());
    return false;
  }

  const auto& object = command.commandObject.objectValue();
  const auto app_it = object.find("app");
  if (app_it == object.end() || !app_it->second.isString()) {
    LOG_ERROR("RtmpConnection connect command missing app on {}", conn->name());
    return false;
  }

  context->setApp(app_it->second.stringValue());

  const auto tc_url_it = object.find("tcUrl");
  if (tc_url_it != object.end() && tc_url_it->second.isString()) {
    context->setTcUrl(tc_url_it->second.stringValue());
  }

  const auto object_encoding_it = object.find("objectEncoding");
  if (object_encoding_it != object.end() &&
      object_encoding_it->second.isNumber()) {
    context->setObjectEncoding(object_encoding_it->second.numberValue());
  }

  LOG_INFO(
      "RtmpConnection handling connect on {}: app={}, tcUrl={}, objectEncoding={}",
      conn->name(), context->app(), context->tcUrl(),
      context->objectEncoding());

  sendWindowAcknowledgementSize(conn, context->acknowledgementWindow());
  sendSetPeerBandwidth(conn, context->peerBandwidth());
  sendSetChunkSize(conn, context->outChunkSize());
  sendConnectSuccess(conn, command);
  return true;
}

bool RtmpConnection::handleCreateStream(const TcpConnectionPtr& conn,
                                        RtmpConnectionContext* context,
                                        const RtmpCommandMessage& command) {
  const uint32_t stream_id = context->allocateNextStreamId();
  context->setStreamId(stream_id);

  LOG_INFO("RtmpConnection handling createStream on {}: streamId={}",
           conn->name(), stream_id);

  sendCreateStreamResult(conn, command, stream_id);
  return true;
}

void RtmpConnection::sendWindowAcknowledgementSize(const TcpConnectionPtr& conn,
                                                   uint32_t window_size) const {
  std::string payload;
  AppendUint32BE(&payload, window_size);
  const RtmpMessage message{
      .timestamp = 0,
      .messageLength = static_cast<uint32_t>(payload.size()),
      .typeId = kMessageTypeWindowAcknowledgementSize,
      .messageStreamId = 0,
      .chunkStreamId = kChunkStreamIdControl,
      .payload = std::move(payload),
  };
  conn->send(RtmpChunkWriter::EncodeMessage(message, kDefaultChunkSize));
}

void RtmpConnection::sendSetPeerBandwidth(const TcpConnectionPtr& conn,
                                          uint32_t peer_bandwidth) const {
  std::string payload;
  AppendUint32BE(&payload, peer_bandwidth);
  payload.push_back('\x02');
  const RtmpMessage message{
      .timestamp = 0,
      .messageLength = static_cast<uint32_t>(payload.size()),
      .typeId = kMessageTypeSetPeerBandwidth,
      .messageStreamId = 0,
      .chunkStreamId = kChunkStreamIdControl,
      .payload = std::move(payload),
  };
  conn->send(RtmpChunkWriter::EncodeMessage(message, kDefaultChunkSize));
}

void RtmpConnection::sendSetChunkSize(const TcpConnectionPtr& conn,
                                      uint32_t chunk_size) const {
  std::string payload;
  AppendUint32BE(&payload, chunk_size);
  const RtmpMessage message{
      .timestamp = 0,
      .messageLength = static_cast<uint32_t>(payload.size()),
      .typeId = kMessageTypeSetChunkSize,
      .messageStreamId = 0,
      .chunkStreamId = kChunkStreamIdControl,
      .payload = std::move(payload),
  };
  conn->send(RtmpChunkWriter::EncodeMessage(message, kDefaultChunkSize));
}

void RtmpConnection::sendConnectSuccess(const TcpConnectionPtr& conn,
                                        const RtmpCommandMessage& command) const {
  Amf0Encoder encoder;
  encoder.appendString("_result");
  encoder.appendNumber(command.transactionId);

  Amf0Value::Object properties;
  properties.emplace("fmsVer", Amf0Value::String("FMS/4,5,0,297"));
  properties.emplace("capabilities", Amf0Value::Number(255.0));
  properties.emplace("mode", Amf0Value::Number(1.0));
  encoder.appendObject(properties);

  Amf0Value::Object information;
  information.emplace("level", Amf0Value::String("status"));
  information.emplace("code",
                      Amf0Value::String("NetConnection.Connect.Success"));
  information.emplace("description",
                      Amf0Value::String("Connection succeeded."));
  information.emplace("objectEncoding", Amf0Value::Number(0.0));
  encoder.appendObject(information);

  const std::string payload = encoder.takeData();
  const RtmpMessage message{
      .timestamp = 0,
      .messageLength = static_cast<uint32_t>(payload.size()),
      .typeId = kMessageTypeCommandAmf0,
      .messageStreamId = 0,
      .chunkStreamId = kChunkStreamIdCommand,
      .payload = payload,
  };
  conn->send(RtmpChunkWriter::EncodeMessage(message, kDefaultChunkSize));
}

void RtmpConnection::sendCreateStreamResult(const TcpConnectionPtr& conn,
                                            const RtmpCommandMessage& command,
                                            uint32_t stream_id) const {
  Amf0Encoder encoder;
  encoder.appendString("_result");
  encoder.appendNumber(command.transactionId);
  encoder.appendNull();
  encoder.appendNumber(static_cast<double>(stream_id));

  const std::string payload = encoder.takeData();
  const RtmpMessage message{
      .timestamp = 0,
      .messageLength = static_cast<uint32_t>(payload.size()),
      .typeId = kMessageTypeCommandAmf0,
      .messageStreamId = 0,
      .chunkStreamId = kChunkStreamIdCommand,
      .payload = payload,
  };
  conn->send(RtmpChunkWriter::EncodeMessage(message, kDefaultChunkSize));
}

}  // namespace rmuduo::rtmp
