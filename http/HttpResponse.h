#pragma once

#include <rmuduo/net/Buffer.h>

#include <map>
#include <string>
#include <string_view>

using namespace rmuduo;

class HttpResponse {
 public:
  enum HttpStatusCode {
    kUnknown,
    k200Ok = 200,
    k301MovedPermanently = 301,
    k400BadRequest = 400,
    k401Unauthorized = 401,
    k404NotFound = 404,
  };

  explicit HttpResponse(bool close)
      : statusCode_(kUnknown), closeConnection_(close) {}
  void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
  void setStatusMessage(std::string_view message) { statusMessage_ = message; }
  void setCloseConnection(bool on) { closeConnection_ = on; }
  bool closeConnection() const { return closeConnection_; }
  void setContentType(std::string_view contentType) {
    addHeader("Content-Type", contentType);
  }
  void addHeader(std::string_view field, std::string_view value) {
    headers_[std::string(field)] = std::string(value);
  }

  void setBody(std::string_view body) { body_ = body; }
  void appendToBuffer(Buffer* output) const;

 private:
  std::map<std::string, std::string> headers_;
  HttpStatusCode statusCode_;

  std::string statusMessage_;
  bool closeConnection_;
  std::string body_;
};

/**
[ 状态行 Status Line ]
内容:  HTTP/1.1       200         OK            \r\n
对应:  [Version]      [Status]    [Reason]      [CRLF]
       (协议版本)      (状态码)    (状态描述)

[ 响应首部 Response Headers ]
内容:  Content-Type: text/plain\r\n
对应:  [Header Key: Value][CRLF]

内容:  Content-Length: 13\r\n
对应:  [Header Key: Value][CRLF]

内容:  Server: MyCppServer/1.0\r\n
对应:  [Header Key: Value][CRLF]

[ 空行 Empty Line ]
内容:  \r\n
对应:  [CRLF] (标记 Header 结束，Body 开始)

[ 响应主体 Message Body ]
内容:  Hello, World!
对应:  [Entity Body / Payload]

HTTP/1.1 200 OK\r\n
Content-Type: text/plain\r\n
Content-Length: 13\r\n
Connection: close\r\n
\r\n
Hello, World!
*/
