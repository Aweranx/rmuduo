#include "HttpResponse.h"
#include <format>

using namespace rmuduo;

void HttpResponse::appendToBuffer(Buffer *output) const {
  std::string buf =
      std::format("HTTP/1.1 {} {}\r\n", (int)statusCode_, statusMessage_);
  output->append(buf);

  if (closeConnection_) {
    output->append("Connection: close\r\n");
  } else {
    buf = std::format("Content-Length: {}\r\n", body_.size());
    output->append(buf);
    output->append("Connection: Keep-Alive\r\n");
  }

  for (const auto &header : headers_) {
    buf = std::format("{}: {}\r\n", header.first, header.second);
    output->append(buf);
  }
  output->append("\r\n");
  output->append(body_);
}