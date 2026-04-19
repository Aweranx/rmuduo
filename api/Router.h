#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "HttpRequest.h"
#include "HttpResponse.h"

namespace api {

class Router {
 public:
  using Handler = std::function<void(const HttpRequest&, HttpResponse*)>;

  void get(const std::string& path, Handler handler);
  void post(const std::string& path, Handler handler);
  void addRoute(HttpRequest::Method method, const std::string& path,
                Handler handler);

  void route(const HttpRequest& request, HttpResponse* response) const;

 private:
  struct RouteKey {
    HttpRequest::Method method;
    std::string path;

    bool operator==(const RouteKey& other) const {
      return method == other.method && path == other.path;
    }
  };

  struct RouteKeyHash {
    size_t operator()(const RouteKey& key) const;
  };

  std::unordered_map<RouteKey, Handler, RouteKeyHash> routes_;
};

}  // namespace api
