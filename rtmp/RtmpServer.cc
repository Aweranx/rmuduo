#include "RtmpServer.h"

#include <any>

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

std::shared_ptr<RtmpSession> RtmpServer::getOrCreateSession(
    const std::string& stream_key) {
  std::lock_guard<std::mutex> lock(mutex_);
  return sessionManager_.getOrCreate(stream_key);
}

void RtmpServer::detachConnectionFromSession(const TcpConnectionPtr& conn,
                                             RtmpConnectionContext* context) {
  cleanupConnection(conn, context);
  if (context == nullptr) {
    return;
  }

  context->clearSession();
  context->setRole(ConnectionRole::kUnknown);
  context->setStreamId(0);
  context->setStreamName({});
  context->setPublishType({});
}

void RtmpServer::onConnection(const TcpConnectionPtr& conn) {
  if (conn->connected()) {
    conn->setContext(RtmpConnectionContext{});
    auto* context = getContext(conn);
    if (context != nullptr) {
      context->bindHandler(std::make_shared<RtmpConnection>(this));
    }
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
  auto* context = getContext(conn);
  if (context == nullptr) {
    LOG_ERROR("RtmpServer missing connection context for {}", conn->name());
    conn->shutdown();
    return;
  }

  auto handler = context->handler();
  if (!handler) {
    LOG_ERROR("RtmpServer missing connection handler for {}", conn->name());
    conn->shutdown();
    return;
  }

  if (!handler->onMessage(conn, buf, receiveTime, context)) {
    conn->shutdown();
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
