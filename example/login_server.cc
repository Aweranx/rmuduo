#include "HttpServer.h"
#include "JsonResponse.h"
#include "Router.h"

#include <rmuduo/net/EventLoop.h>
#include <rmuduo/net/InetAddress.h>

#include <cstdlib>

using namespace rmuduo;

int main(int argc, char* argv[]) {
  uint16_t port = 8888;
  if (argc > 1) {
    port = static_cast<uint16_t>(std::atoi(argv[1]));
  }

  EventLoop loop;
  api::Router router;

  router.get("/api/health", [](const HttpRequest&, HttpResponse* response) {
    response->setStatusCode(HttpResponse::k200Ok);
    response->setStatusMessage("OK");
    response->setContentType("application/json");
    response->setBody(api::makeOk("{\"service\":\"login_server\"}"));
  });

  HttpServer server(&loop, InetAddress(port), "login_server");
  server.setHttpCallback(
      [&router](const HttpRequest& request, HttpResponse* response) {
        router.route(request, response);
      });

  server.start();
  loop.loop();
}
