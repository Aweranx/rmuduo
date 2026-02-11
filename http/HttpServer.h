#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"
#include <rmuduo/net/TcpServer.h>

using namespace rmuduo;

class HttpServer : noncopyable {
public:
  using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

  HttpServer(EventLoop *loop, const InetAddress &listenAddr,
             const std::string &name,
             TcpServer::Option option = TcpServer::kNoReusePort);

  EventLoop *getLoop() const { return server_.getLoop(); }

  /// Not thread safe, callback be registered before calling start().
  void setHttpCallback(const HttpCallback& cb) { httpCallback_ = cb; }

  void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }

  void start();

private:
  void onConnection(const TcpConnectionPtr& conn);
  void onMessage(const TcpConnectionPtr& conn, Buffer* buf,
                 Timestamp receiveTime);
  void onRequest(const TcpConnectionPtr&, const HttpRequest&);

  TcpServer server_;
  HttpCallback httpCallback_;
};