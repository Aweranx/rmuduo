#pragma once
#include <rmuduo/net/TcpServer.h>
#include <rmuduo/net/Timestamp.h>

#include <cctype>
#include <cstddef>
#include <map>
#include <string>
using namespace rmuduo;


class HttpRequest {
 public:
  enum Method { kInvalid, kGet, kPost, kHead, kPut, kDelete };
  enum Version { kUnknown, kHttp10, kHttp11 };

  HttpRequest() : method_(kInvalid), version_(kUnknown) {}

  void setVersion(Version v) { version_ = v; }
  Version getVersion() const { return version_; }
  bool setMethod(const char* start, const char* end) {
    static const std::unordered_map<std::string_view, Method> kMethods = {
        {"GET", kGet},
        {"POST", kPost},
        {"HEAD", kHead},
        {"PUT", kPut},
        {"DELETE", kDelete}};
    std::string_view m(start, end - start);
    auto it = kMethods.find(m);
    if (it != kMethods.end()) {
      method_ = it->second;
      return true;
    }
    method_ = kInvalid;
    return false;
  }
  Method method() const { return method_; }
  std::string_view method2Str() const {
    static constexpr std::string_view kMethodStr[] = {
        "UNKNOWN", "GET", "POST", "HEAD", "PUT", "DELETE"};
    if (method_ < (sizeof(kMethodStr) / sizeof(kMethodStr[0]))) {
      return kMethodStr[method_];
    }
    return kMethodStr[0];
  }

  void setPath(const char* start, const char* end) { path_.assign(start, end); }
  std::string_view path() const { return path_; }
  void setQuery(const char* start, const char* end) {
    query_.assign(start, end);
  }
  std::string_view query() const { return query_; }
  void setReceiveTime(Timestamp t) { receiveTime_ = t; }
  Timestamp receiveTime() const { return receiveTime_; }
  // colon是冒号
  void addHeader(const char* start, const char* colon, const char* end) {
    std::string_view field(start, static_cast<size_t>(colon - start));
    ++colon;
    while (colon < end && std::isspace(*colon)) ++colon;
    while (colon < end && isspace(*(end - 1))) --end;
    std::string_view value(colon, static_cast<size_t>(end - colon));
    headers_[std::string(field)] = std::string(value);
  }
  std::string getHeader(std::string_view field) const {
    std::string result;
    auto it = headers_.find(std::string(field));
    if (it != headers_.end()) {
      result = it->second;
    }
    return result;
  }

  const std::map<std::string, std::string>& headers() const { return headers_; }
  void swap(HttpRequest& that) {
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    receiveTime_.swap(that.receiveTime_);
    headers_.swap(that.headers_);
  }

 private:
  Method method_;
  Version version_;
  std::string path_;
  std::string query_;
  Timestamp receiveTime_;
  std::map<std::string, std::string> headers_;
};

/**
[ 请求行 Request Line ]
内容:  POST      /api/login      HTTP/1.1       \r\n
对应:  [Method]   [Path/URL]      [Version]      [CRLF]

[ 请求首部 Request Headers ]
内容:  Host: www.example.com\r\n
对应:  [Header Key: Value][CRLF]

内容:  Content-Type: application/json\r\n
对应:  [Header Key: Value][CRLF]

内容:  Content-Length: 32\r\n
对应:  [Header Key: Value][CRLF]

[ 空行 Empty Line ]
内容:  \r\n
对应:  [CRLF] (标记 Header 结束，Body 开始)

[ 报文主体 Message Body ]
内容:  {"username": "admin", "id": 123}
对应:  [Entity Body / Payload]

POST /api/login HTTP/1.1\r\n
Host: www.example.com\r\n
User-Agent: Mozilla/5.0\r\n
Content-Type: application/json\r\n
Content-Length: 32\r\n
Connection: close\r\n
\r\n
{"username": "admin", "id": 123}
*/