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
#include "RtmpServer.h"

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

  if (message.typeId == kMessageTypeDataAmf0 ||
      message.typeId == kMessageTypeAudio ||
      message.typeId == kMessageTypeVideo) {
    return handleMediaMessage(conn, message, context);
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

  if (command.name == "publish") {
    return handlePublish(conn, context, command);
  }

  if (command.name == "play") {
    return handlePlay(conn, context, command);
  }

  if (command.name == "deleteStream" || command.name == "closeStream") {
    return handleDeleteStream(conn, context, command);
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

bool RtmpConnection::handlePublish(const TcpConnectionPtr& conn,
                                   RtmpConnectionContext* context,
                                   const RtmpCommandMessage& command) {
  if (server_ == nullptr) {
    LOG_ERROR("RtmpConnection has no server reference on {}", conn->name());
    return false;
  }

  if (context->streamId() == 0) {
    LOG_ERROR("RtmpConnection publish arrived before createStream on {}",
              conn->name());
    return false;
  }

  if (command.messageStreamId != context->streamId()) {
    LOG_ERROR(
        "RtmpConnection publish stream id mismatch on {}: cmd={}, expected={}",
        conn->name(), command.messageStreamId, context->streamId());
    return false;
  }

  if (command.arguments.empty() || !command.arguments[0].isString()) {
    LOG_ERROR("RtmpConnection publish missing stream name on {}", conn->name());
    return false;
  }

  context->setStreamName(command.arguments[0].stringValue());
  if (command.arguments.size() >= 2 && command.arguments[1].isString()) {
    context->setPublishType(command.arguments[1].stringValue());
  }

  const std::string stream_key = context->streamKey();
  if (stream_key.empty()) {
    LOG_ERROR("RtmpConnection publish stream key is empty on {}", conn->name());
    return false;
  }

  auto session = server_->getOrCreateSession(stream_key);
  if (!session->setPublisher(conn)) {
    LOG_INFO(
        "RtmpConnection publish rejected on {}: stream {} already has publisher",
        conn->name(), stream_key);
    sendOnStatus(conn, context->streamId(), "NetStream.Publish.BadName",
                 "Stream already publishing.", true);
    return true;
  }

  context->setRole(ConnectionRole::kPublisher);
  context->bindSession(session);

  LOG_INFO("RtmpConnection publish start on {}: streamKey={}, publishType={}",
           conn->name(), stream_key, context->publishType());
  sendOnStatus(conn, context->streamId(), "NetStream.Publish.Start",
               "Start publishing.", false);
  return true;
}

bool RtmpConnection::handlePlay(const TcpConnectionPtr& conn,
                                RtmpConnectionContext* context,
                                const RtmpCommandMessage& command) {
  if (server_ == nullptr) {
    LOG_ERROR("RtmpConnection has no server reference on {}", conn->name());
    return false;
  }

  if (context->streamId() == 0) {
    LOG_ERROR("RtmpConnection play arrived before createStream on {}",
              conn->name());
    return false;
  }

  if (command.messageStreamId != context->streamId()) {
    LOG_ERROR(
        "RtmpConnection play stream id mismatch on {}: cmd={}, expected={}",
        conn->name(), command.messageStreamId, context->streamId());
    return false;
  }

  if (command.arguments.empty() || !command.arguments[0].isString()) {
    LOG_ERROR("RtmpConnection play missing stream name on {}", conn->name());
    return false;
  }

  // play 也依赖 app + streamName 定位会话，这里先只建立 player 关系，
  // metadata、首帧缓存和实时转发放到后续步骤里补。
  context->setStreamName(command.arguments[0].stringValue());
  const std::string stream_key = context->streamKey();
  if (stream_key.empty()) {
    LOG_ERROR("RtmpConnection play stream key is empty on {}", conn->name());
    return false;
  }

  auto session = server_->getOrCreateSession(stream_key);
  session->addPlayer(conn);
  context->setRole(ConnectionRole::kPlayer);
  context->bindSession(session);

  LOG_INFO("RtmpConnection play start on {}: streamKey={}, players={}",
           conn->name(), stream_key, session->playerCount());
  sendOnStatus(conn, context->streamId(), "NetStream.Play.Start",
               "Start playing.", false);
  session->replayCachedMessagesToPlayer(conn, context->outChunkSize());
  return true;
}

bool RtmpConnection::handleMediaMessage(const TcpConnectionPtr& conn,
                                        const RtmpMessage& message,
                                        RtmpConnectionContext* context) {
  if (context->role() != ConnectionRole::kPublisher) {
    LOG_INFO("RtmpConnection ignores media message from non-publisher {}",
             conn->name());
    return true;
  }

  auto session = context->session();
  if (!session) {
    LOG_ERROR("RtmpConnection publisher {} has no bound session", conn->name());
    return false;
  }

  // 会话层同时负责两件事：
  // 1. 把实时 metadata/audio/video 广播给现有 player
  // 2. 缓存 metadata、sequence header 和最近一个 GOP，供新 player 补发
  session->onMediaMessage(message, context->outChunkSize());
  LOG_INFO(
      "RtmpConnection broadcast media on {}: streamKey={}, type={}, players={}",
      conn->name(), session->streamKey(), message.typeId, session->playerCount());
  return true;
}

bool RtmpConnection::handleDeleteStream(const TcpConnectionPtr& conn,
                                        RtmpConnectionContext* context,
                                        const RtmpCommandMessage& command) {
  if (server_ == nullptr) {
    LOG_ERROR("RtmpConnection has no server reference on {}", conn->name());
    return false;
  }

  if (context->role() == ConnectionRole::kUnknown || !context->session()) {
    LOG_INFO("RtmpConnection {} ignored on {} without active session",
             command.name, conn->name());
    return true;
  }

  if (command.messageStreamId != 0 && context->streamId() != 0 &&
      command.messageStreamId != context->streamId()) {
    LOG_ERROR(
        "RtmpConnection {} stream id mismatch on {}: cmd={}, expected={}",
        command.name, conn->name(), command.messageStreamId, context->streamId());
    return false;
  }

  LOG_INFO("RtmpConnection handling {} on {}: streamKey={}", command.name,
           conn->name(), context->streamKey());
  server_->detachConnectionFromSession(conn, context);
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

void RtmpConnection::sendOnStatus(const TcpConnectionPtr& conn,
                                  uint32_t message_stream_id,
                                  std::string_view code,
                                  std::string_view description,
                                  bool error) const {
  Amf0Encoder encoder;
  encoder.appendString("onStatus");
  encoder.appendNumber(0.0);
  encoder.appendNull();

  Amf0Value::Object information;
  information.emplace("level",
                      Amf0Value::String(error ? "error" : "status"));
  information.emplace("code", Amf0Value::String(std::string(code)));
  information.emplace("description",
                      Amf0Value::String(std::string(description)));
  encoder.appendObject(information);

  const std::string payload = encoder.takeData();
  const RtmpMessage message{
      .timestamp = 0,
      .messageLength = static_cast<uint32_t>(payload.size()),
      .typeId = kMessageTypeCommandAmf0,
      .messageStreamId = message_stream_id,
      .chunkStreamId = kChunkStreamIdCommand,
      .payload = payload,
  };
  conn->send(RtmpChunkWriter::EncodeMessage(message, kDefaultChunkSize));
}

}  // namespace rmuduo::rtmp
