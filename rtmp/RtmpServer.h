#pragma once

#include <mutex>
#include <string>

#include <rmuduo/net/TcpServer.h>

#include "RtmpConnection.h"
#include "RtmpConnectionContext.h"
#include "RtmpSessionManager.h"

namespace rmuduo::rtmp {

class RtmpServer : noncopyable {
 public:
  RtmpServer(EventLoop* loop, const InetAddress& listen_addr,
             const std::string& name,
             TcpServer::Option option = TcpServer::kNoReusePort);

  EventLoop* getLoop() const { return server_.getLoop(); }
  void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
  void start();

 private:
  // 连接建立/关闭时挂接和清理 RTMP 上下文。
  void onConnection(const TcpConnectionPtr& conn);
  // 接入层只负责把输入转发给连接级协议处理器。
  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime);

  RtmpConnectionContext* getContext(const TcpConnectionPtr& conn) const;
  void cleanupConnection(const TcpConnectionPtr& conn,
                         RtmpConnectionContext* context);

  TcpServer server_;
  mutable std::mutex mutex_;
  RtmpSessionManager sessionManager_;
};

}  // namespace rmuduo::rtmp
