#pragma once

#include <map>
#include <rmuduo/net/Buffer.h>
#include <string_view>

using namespace rmuduo;

class HttpResponse {
public:
  enum HttpStatusCode {
    kUnknown,
    k200Ok = 200,
    k301MovedPermanently = 301,
    k400BadRequest = 400,
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

  void addHeader(std::string_view key, std::string_view value) {
    headers_[std::string(key)] = std::string(value);
  }

  void setBody(std::string_view body) { body_ = body; }

  void appendToBuffer(Buffer *output) const;

private:
  std::map<std::string, std::string> headers_;
  HttpStatusCode statusCode_;

  std::string statusMessage_;
  bool closeConnection_;
  std::string body_;
};