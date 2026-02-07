#include "net/TcpServer.h"
#include "net/EventLoop.h"
#include "net/Logger.h"
#include "net/Buffer.h"
#include "net/Timestamp.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <string>
#include <functional>

// 使用 rmuduo 命名空间简化代码
using namespace rmuduo;
using namespace std::placeholders;

class EchoServer {
public:
    EchoServer(EventLoop* loop,
               const InetAddress& listenAddr,
               const std::string& nameArg)
        : server_(loop, listenAddr, nameArg),
          loop_(loop) {
        
        // 注册连接回调
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, _1));
        
        // 注册消息回调
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, _1, _2, _3));

        // 设置线程数：1个主Reactor负责Accept，3个子Reactor负责IO
        server_.setThreadNum(5);
    }

    void start() {
        server_.start();
    }

private:
    // 当连接建立或断开时的回调
    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO("EchoServer - Connection UP : {}", conn->peerAddress().toIpPort());
        } else {
            LOG_INFO("EchoServer - Connection DOWN : {}", conn->peerAddress().toIpPort());
        }
    }

    // 当收到对端数据时的回调
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
    std::string msg = buf->retrieveAllAsString();
    
    // 简单的 HTTP 响应
    std::string httpResponse = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 38\r\n"
        "Connection: Keep-Alive\r\n"
        "\r\n"
        "<html><body>Hello Muduo!</body></html>";

    if (msg.find("GET") != std::string::npos) {
        conn->send(httpResponse);
        // conn->shutdown(); // HTTP 短连接通常在发送后关闭
    } else {
        conn->send(msg); // 兼容原有的 Echo 逻辑
    }
}

    TcpServer server_;
    EventLoop* loop_;
};

int main() {
    // ---- 开启异步日志核心配置 ----
    // 1. 初始化异步线程池：8192是队列长度，1是后台处理线程数
    spdlog::init_thread_pool(8192, 1);
    
    // 2. 创建异步颜色控制台日志器
    auto logger = spdlog::stdout_color_mt<spdlog::async_factory>("async_logger");
    
    // 3. 设置为全局默认，这样你代码里的 LOG_INFO 都会走这个异步 logger
    spdlog::set_default_logger(logger);

    // 4. (可选) 压测时设置为 warn 级别，进一步减少后台线程压力
    // spdlog::set_level(spdlog::level::warn); 
    // ----------------------------

    EventLoop loop;
    InetAddress addr(8888); 
    EchoServer server(&loop, addr, "EchoServer-Test");
    
    LOG_INFO("EchoServer is starting on 8888 (Async Logging)...");
    server.start(); 

    loop.loop();    
    
    return 0;
}