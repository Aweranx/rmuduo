#include "RtmpServer.h"

#include <any>
#include <utility>

#include <rmuduo/net/Buffer.h>
#include <rmuduo/net/EventLoop.h>
#include <rmuduo/net/Logger.h>

namespace rmuduo::rtmp {

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

  // 握手完成后，这里就是后续 chunk 解析器的接入点。
  LOG_INFO("RtmpServer has {} post-handshake bytes pending on {}", 
           buf->readableBytes(), conn->name());
  buf->retrieveAll();
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
