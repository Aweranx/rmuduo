#include <cstdlib>
#include <iostream>
#include <string>

#include <rmuduo/net/EventLoop.h>
#include <rmuduo/net/InetAddress.h>
#include <rmuduo/net/Logger.h>
#include "rtmp/RtmpServer.h"

using namespace rmuduo;

namespace {

int ParseOrDefault(const char* text, int default_value) {
  if (text == nullptr) {
    return default_value;
  }
  const int value = std::atoi(text);
  return value > 0 ? value : default_value;
}

void PrintUsage(const char* program) {
  std::cout << "Usage: " << program << " [port] [threads]\n";
  std::cout << "Default port: 1935, default threads: 2\n\n";
  std::cout << "FFmpeg push example:\n";
  std::cout
      << "  ffmpeg -re -stream_loop -1 -i input.mp4 -c copy -f flv "
         "rtmp://127.0.0.1:1935/live/test\n\n";
  std::cout << "FFplay pull example:\n";
  std::cout << "  ffplay rtmp://127.0.0.1:1935/live/test\n\n";
  std::cout << "OBS setup:\n";
  std::cout << "  Server: rtmp://127.0.0.1:1935/live\n";
  std::cout << "  Stream Key: test\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc > 1 && std::string(argv[1]) == "--help") {
    PrintUsage(argv[0]);
    return 0;
  }

  const int port = argc > 1 ? ParseOrDefault(argv[1], 1935) : 1935;
  const int threads = argc > 2 ? ParseOrDefault(argv[2], 2) : 2;

  EventLoop loop;
  InetAddress listen_addr(static_cast<uint16_t>(port));
  rmuduo::rtmp::RtmpServer server(&loop, listen_addr, "RtmpServer-Test");
  server.setThreadNum(threads);

  LOG_INFO("RTMP test server starting on port {}, threads={}", port, threads);
  LOG_INFO("Push URL: rtmp://127.0.0.1:{}/live/test", port);
  LOG_INFO("Run with --help to print ffmpeg/OBS examples");

  server.start();
  loop.loop();
  return 0;
}
