#include "HttpContext.h"

#include <algorithm>

#include "HttpRequest.h"

// 内容:  POST      /api/login      HTTP/1.1       \r\n
// 对应:  [Method]   [Path/URL]      [Version]      [CRLF]
// 内容:  GET          /search?q=apple     HTTP/1.1      \r\n
// 对应:  [Method]      [Path / Query]      [Version]     [CRLF]
//        (获取资源)     (路径与参数)         (协议版本)
bool HttpContext::processRequestLine(const char* begin, const char* end) {
  bool succeed = false;
  const char* start = begin;  // *start='G'
  // 解析method *space=' '
  const char* space = std::find(start, end, ' ');
  if (space != end && request_.setMethod(start, space)) {
    start = space + 1;
    // 解析path *(start-space)="/search?q=apple"
    space = std::find(start, end, ' ');
    if (space != end) {
      // 区分GET和POST
      const char* question = std::find(start, space, '?');
      if (question != space) {  // GET /search?q=apple
        // *question='?'
        request_.setPath(start, question);   // /search
        request_.setQuery(question, space);  // ?q=apple
      } else {                               // POST /api/login
        request_.setPath(start, space);
      }

      start = space + 1;
      // 解析version *(start-end)="HTTP/1.1"
      succeed = (end - start == 8) && std::equal(start, end - 1, "HTTP/1.");
      if (succeed) {
        if (*(end - 1) == '1') {
          request_.setVersion(HttpRequest::kHttp11);
        } else if (*(end - 1) == '0') {
          request_.setVersion(HttpRequest::kHttp10);
        } else {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}

bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime) {
  bool ok = true;
  bool hasMore = true;
  while (hasMore) {
    // Request Line
    if (state_ == kExpectRequestLine) {
      const char* crlf = buf->findCRLF();
      if (crlf) {
        ok = processRequestLine(buf->peek(), crlf);
        if (ok) {
          request_.setReceiveTime(receiveTime);
          buf->retrieveUntil(crlf + 2);
          state_ = kExpectHeaders;
        } else {
          hasMore = false;
        }
      } else {
        hasMore = false;
      }
      // Headers
    } else if (state_ == kExpectHeaders) {
      const char* crlf = buf->findCRLF();
      if (crlf) {
        const char* colon = std::find(buf->peek(), crlf, ':');
        if (colon != crlf) {
          request_.addHeader(buf->peek(), colon, crlf);
        } else {
          state_ = kGotAll;
          hasMore = false;
        }
        buf->retrieveUntil(crlf + 2);
      } else {
        hasMore = false;
      }
    } else if (state_ == kExpectBody) {
      // add code for body
    }
  }
  return ok;
}