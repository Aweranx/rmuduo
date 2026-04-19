#pragma once

#include <rmuduo/net/Timestamp.h>

#include <cstddef>

#include "HttpRequest.h"

using namespace rmuduo;

class HttpContext {
 public:
  enum HttpRequestParseState {
    kExpectRequestLine,
    kExpectHeaders,
    kExpectBody,
    kGotAll,
  };

  HttpContext() : state_(kExpectRequestLine), contentLength_(0) {}

  bool parseRequest(Buffer* buf, Timestamp receiveTime);
  bool gotAll() const { return state_ == kGotAll; }

  void reset() {
    state_ = kExpectRequestLine;
    contentLength_ = 0;
    HttpRequest dummy;
    request_.swap(dummy);
  }

  const HttpRequest& request() const { return request_; }
  HttpRequest& request() { return request_; }

 private:
  bool processRequestLine(const char* begin, const char* end);
  bool parseContentLength();

  HttpRequestParseState state_;
  HttpRequest request_;
  size_t contentLength_;
};
