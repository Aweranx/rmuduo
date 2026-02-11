#include "HttpContext.h"

bool HttpContext::processRequestLine(const char *begin, const char *end) {
  bool succeed = false;
  const char *start = begin;
  // 解析method *(start-space) == GET/POST
  const char *space = std::find(start, end, ' ');
  if (space != end && request_.setMethod(start, space)) {
    start = space + 1;
    space = std::find(start, end, ' ');
    if (space != end) {
      // GET OR POST
      const char *question = std::find(start, space, '?');
      if (question != space) {
        // *(start - space) == /path?query=apple
        request_.setPath(start, question);
        request_.setQuery(question, space);
      } else {
        // *(start - space) == /path
        request_.setPath(start, space);
      }
      start = space + 1;
      // *(start - end) == HTTP/1.1
      succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
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

// return false if any error
bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime) {
  bool ok = true;
  bool hasMore = true;
  while (hasMore) {
    if (state_ == kExpectRequestLine) {
      const char *crlf = buf->findCRLF();
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
    } else if (state_ == kExpectHeaders) {
      const char *crlf = buf->findCRLF();
      if (crlf) {
        const char *colon = std::find(buf->peek(), crlf, ':');
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
    }
  }
  return ok;
}
