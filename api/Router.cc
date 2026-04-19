#include "Router.h"

#include "JsonResponse.h"

namespace api {

size_t Router::RouteKeyHash::operator()(const RouteKey& key) const {
  size_t methodHash = std::hash<int>{}(static_cast<int>(key.method));
  size_t pathHash = std::hash<std::string>{}(key.path);
  return methodHash ^ (pathHash + 0x9e3779b9 + (methodHash << 6) +
                       (methodHash >> 2));
}

void Router::get(const std::string& path, Handler handler) {
  addRoute(HttpRequest::kGet, path, std::move(handler));
}

void Router::post(const std::string& path, Handler handler) {
  addRoute(HttpRequest::kPost, path, std::move(handler));
}

void Router::addRoute(HttpRequest::Method method, const std::string& path,
                      Handler handler) {
  routes_[RouteKey{method, path}] = std::move(handler);
}

void Router::route(const HttpRequest& request, HttpResponse* response) const {
  auto it = routes_.find(RouteKey{request.method(), std::string(request.path())});
  if (it == routes_.end()) {
    response->setStatusCode(HttpResponse::k404NotFound);
    response->setStatusMessage("Not Found");
    response->setContentType("application/json");
    response->setBody(makeError(404, "not found"));
    return;
  }

  it->second(request, response);
}

}  // namespace api
