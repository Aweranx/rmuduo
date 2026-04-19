#include "HttpContext.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string_view>

#include "HttpRequest.h"

namespace {

bool equalCaseInsensitive(std::string_view lhs, std::string_view rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                    [](unsigned char a, unsigned char b) {
                      return std::tolower(a) == std::tolower(b);
                    });
}

}  // namespace

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

bool HttpContext::parseContentLength() {
  contentLength_ = 0;
  for (const auto& header : request_.headers()) {
    if (!equalCaseInsensitive(header.first, "Content-Length")) {
      continue;
    }

    std::string_view value(header.second);
    size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
      ++first;
    }

    size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
      --last;
    }

    value = value.substr(first, last - first);
    if (value.empty()) {
      return false;
    }

    size_t length = 0;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    auto [ptr, ec] = std::from_chars(begin, end, length);
    if (ec != std::errc() || ptr != end) {
      return false;
    }

    contentLength_ = length;
    break;
  }
  return true;
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
          ok = parseContentLength();
          if (!ok) {
            hasMore = false;
          } else if (contentLength_ > 0) {
            state_ = kExpectBody;
          } else {
            state_ = kGotAll;
            hasMore = false;
          }
        }
        if (ok) {
          buf->retrieveUntil(crlf + 2);
        }
      } else {
        hasMore = false;
      }
    } else if (state_ == kExpectBody) {
      if (buf->readableBytes() >= contentLength_) {
        request_.setBody(buf->peek(), buf->peek() + contentLength_);
        buf->retrieve(contentLength_);
        state_ = kGotAll;
      } else {
        hasMore = false;
      }
    } else if (state_ == kGotAll) {
      hasMore = false;
    }
  }
  return ok;
}
