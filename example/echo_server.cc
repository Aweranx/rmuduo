
#include <rmuduo/net/TcpServer.h>


using namespace rmuduo;

class EchoServer {
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr,
             const std::string& nameArg)
      : server_(loop, listenAddr, nameArg), loop_(loop) {
    server_.setThreadNum(5);

    server_.setConnectionCallback([this](TcpConnectionPtr conn) {
      if (conn->connected()) {
        LOG_INFO("EchoServer - {} is UP", conn->peerAddress().toIpPort());
      } else {
        LOG_INFO("EchoServer - {} is DOWN", conn->peerAddress().toIpPort());
      }
    });
    server_.setMessageCallback(
        [this](TcpConnectionPtr conn, Buffer* buf, Timestamp receiveTime) {
          std::string msg = buf->retrieveAllAsString();
          LOG_INFO("{} echo {} bytes at {}", conn->name(), msg.size(),
                   receiveTime.toString());

          // 此时 isInLoopThread 为 true，send 会直接调用 sendInLoop
          conn->send(msg);
        });
  }
  ~EchoServer() = default;

  void start() { server_.start(); }

 private:
  TcpServer server_;
  EventLoop* loop_;
};

int main() {
  LOG_INFO("echoServer starting...");

  EventLoop loop;
  InetAddress addr(8888);

  EchoServer server(&loop, addr, "EchoServer");
  server.start();  // 开启线程池并启动监听

  loop.loop();  // 主线程进入事件循环，等待新连接

  return 0;
}