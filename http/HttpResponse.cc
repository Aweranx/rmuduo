#include "HttpResponse.h"

#include <rmuduo/net/Buffer.h>

#include <format>
#include <string>

using namespace rmuduo;

void HttpResponse::appendToBuffer(Buffer* output) const {
  std::string buf = std::format("HTTP/1.1 {} {}\r\n",
                                static_cast<int>(statusCode_), statusMessage_);
  output->append(buf);

  if (closeConnection_) {
    output->append("Connection: close\r\n");
  } else {
    output->append(std::format("Content-Length: {}\r\n", body_.size()));
    output->append("Connection: Keep-Alive\r\n");
  }
  for (const auto& header : headers_) {
    std::string headerLine =
        std::format("{}: {}\r\n", header.first, header.second);
    output->append(headerLine);
  }

  output->append("\r\n");
  output->append(body_);
}