#include "RtmpServer.h"

#include <any>
#include <cstdint>
#include <utility>
#include <vector>

#include <rmuduo/net/Buffer.h>
#include <rmuduo/net/EventLoop.h>
#include <rmuduo/net/Logger.h>

namespace rmuduo::rtmp {

namespace {

uint32_t ReadUint32BE(const char* data) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(data[0])) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 8) |
         static_cast<uint32_t>(static_cast<uint8_t>(data[3]));
}

}  // namespace

RtmpServer::RtmpServer(EventLoop* loop, const InetAddress& listen_addr,
                       const std::string& name, TcpServer::Option option)
    : server_(loop, listen_addr, name, option) {
  server_.setConnectionCallback(
      std::bind(&RtmpServer::onConnection, this, std::placeholders::_1));
  server_.setMessageCallback(
      std::bind(&RtmpServer::onMessage, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
}

void RtmpServer::start() {
  LOG_INFO("RtmpServer {} starts listening on {}", server_.name(),
           server_.ipPort());
  server_.start();
}

void RtmpServer::onConnection(const TcpConnectionPtr& conn) {
  if (conn->connected()) {
    // 每条 RTMP 连接都有独立的协议状态，连接建立时直接挂一个 context。
    conn->setContext(RtmpConnectionContext{});
    LOG_INFO("RtmpServer accepted connection {} from {}", conn->name(),
             conn->peerAddress().toIpPort());
    return;
  }

  auto* context = getContext(conn);
  cleanupConnection(conn, context);
  LOG_INFO("RtmpServer closed connection {}", conn->name());
}

void RtmpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf,
                           Timestamp receiveTime) {
  (void)receiveTime;
  auto* context = getContext(conn);
  if (context == nullptr) {
    LOG_ERROR("RtmpServer missing connection context for {}", conn->name());
    conn->shutdown();
    return;
  }

  const size_t bytes = buf->readableBytes();
  context->addReceivedBytes(bytes);

  if (!context->handshake().isDone()) {
    // 握手阶段只推进握手状态机，不做任何 RTMP chunk 解析。
    std::string response;
    std::string error_message;
    if (!context->handshake().process(buf, &response, &error_message)) {
      LOG_ERROR("RtmpServer handshake failed on {}: {}", conn->name(),
                error_message);
      conn->shutdown();
      return;
    }

    if (!response.empty()) {
      conn->send(response);
      LOG_INFO("RtmpServer sent {} handshake bytes on {}", response.size(),
               conn->name());
    }

    if (!context->handshake().isDone()) {
      // 数据还不够，等下次 onMessage 继续推进。
      return;
    }

    LOG_INFO("RtmpServer handshake completed on {}", conn->name());
  }

  if (buf->readableBytes() == 0) {
    return;
  }

  std::vector<RtmpMessage> messages;
  std::string error_message;
  if (!context->chunkParser().parse(buf, &messages, &error_message)) {
    LOG_ERROR("RtmpServer chunk parse failed on {}: {}", conn->name(),
              error_message);
    conn->shutdown();
    return;
  }

  for (const auto& message : messages) {
    LOG_INFO(
        "RtmpServer parsed message on {}: csid={}, type={}, len={}, stream={}",
        conn->name(), message.chunkStreamId, message.typeId,
        message.messageLength, message.messageStreamId);

    // 客户端可能先下发 Set Chunk Size，后续 chunk 解析要立即切换到新的入站大小。
    if (message.typeId == kMessageTypeSetChunkSize &&
        message.payload.size() >= sizeof(uint32_t)) {
      const uint32_t chunk_size = ReadUint32BE(message.payload.data());
      context->setInChunkSize(chunk_size);
      context->chunkParser().setInChunkSize(chunk_size);
      LOG_INFO("RtmpServer updated inbound chunk size on {} to {}",
               conn->name(), chunk_size);
    }
  }
}

RtmpConnectionContext* RtmpServer::getContext(
    const TcpConnectionPtr& conn) const {
  return std::any_cast<RtmpConnectionContext>(conn->getMutableContext());
}

void RtmpServer::cleanupConnection(const TcpConnectionPtr& conn,
                                   RtmpConnectionContext* context) {
  if (context == nullptr) {
    return;
  }

  const auto role = context->role();
  const auto stream_key = context->streamKey();
  if (stream_key.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  auto session = sessionManager_.find(stream_key);
  if (!session) {
    return;
  }

  // 连接关闭时按角色从 session 中摘掉，避免残留脏引用。
  if (role == ConnectionRole::kPublisher) {
    session->clearPublisher(conn);
  } else if (role == ConnectionRole::kPlayer) {
    session->removePlayer(conn);
  }

  sessionManager_.removeIfEmpty(stream_key);
}

}  // namespace rmuduo::rtmp
