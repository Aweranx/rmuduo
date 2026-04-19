#include <rmuduo/net/EventLoop.h>
#include <rmuduo/net/InetAddress.h>

#include <cctype>
#include <cstdlib>
#include <format>
#include <optional>
#include <string>

#include "AuthService.h"
#include "AuthTypes.h"
#include "HttpServer.h"
#include "JsonRequest.h"
#include "JsonResponse.h"
#include "Router.h"
#include "RtmpTicketManager.h"
#include "rtmp/RtmpServer.h"

using namespace rmuduo;

namespace {

int authErrorCode(auth::AuthError error) {
  switch (error) {
    case auth::AuthError::kOk:
      return 0;
    case auth::AuthError::kBadRequest:
      return 1000;
    case auth::AuthError::kInvalidCredentials:
      return 1001;
    case auth::AuthError::kUnauthorized:
      return 1002;
    case auth::AuthError::kTokenExpired:
      return 1004;
    case auth::AuthError::kUserDisabled:
      return 1005;
    case auth::AuthError::kUnsupportedRole:
      return 1006;
  }
  return 2000;
}

std::string makeLoginData(const auth::AuthSession& session) {
  return "{" + api::makeField("token", session.token) + "," +
         api::makeField("userId", session.userId) + "," +
         api::makeField("username", session.username) + "," +
         api::makeField("role", auth::roleToString(session.role)) + "," +
         api::makeNumberField("expireAt", session.expireAt) + "}";
}

std::string makeUserData(const auth::AuthSession& session) {
  return "{" + api::makeField("userId", session.userId) + "," +
         api::makeField("username", session.username) + "," +
         api::makeField("role", auth::roleToString(session.role)) + "}";
}

std::string makeStreamKey(std::string_view app, std::string_view streamName) {
  return "/" + std::string(app) + "/" + std::string(streamName);
}

bool isValidStreamName(std::string_view streamName) {
  if (streamName.empty()) {
    return false;
  }

  for (char ch : streamName) {
    bool ok = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' ||
              ch == '-' || ch == '.';
    if (!ok) {
      return false;
    }
  }
  return true;
}

std::string makeRtmpTicketData(const auth::RtmpTicket& ticket,
                               std::string_view host, uint16_t rtmpPort,
                               std::string_view app,
                               std::string_view streamName) {
  std::string url = std::format("rtmp://{}:{}/{}/{}?ticket={}", host, rtmpPort,
                               app, streamName, ticket.ticket);
  std::string data = "{" + api::makeField("streamKey", ticket.streamKey) + ",";
  if (ticket.allowPublish) {
    data += api::makeField("publishUrl", url) + ",";
  }
  if (ticket.allowPlay) {
    data += api::makeField("playUrl", url) + ",";
  }
  data += api::makeNumberField("expireAt", ticket.expireAt) + "}";
  return data;
}

std::optional<std::string> getBearerToken(const HttpRequest& request) {
  std::string authorization = request.getHeader("Authorization");
  constexpr std::string_view kPrefix = "Bearer ";
  if (authorization.size() <= kPrefix.size() ||
      authorization.compare(0, kPrefix.size(), kPrefix) != 0) {
    return std::nullopt;
  }
  return authorization.substr(kPrefix.size());
}

void setJson(HttpResponse* response, HttpResponse::HttpStatusCode status,
             std::string_view statusMessage, std::string_view body) {
  response->setStatusCode(status);
  response->setStatusMessage(statusMessage);
  response->setContentType("application/json");
  response->setBody(body);
}

void setUnauthorized(HttpResponse* response) {
  setJson(
      response, HttpResponse::k401Unauthorized, "Unauthorized",
      api::makeError(authErrorCode(auth::AuthError::kUnauthorized),
                     auth::authErrorMessage(auth::AuthError::kUnauthorized)));
}

}  // namespace

int main(int argc, char* argv[]) {
  uint16_t port = 8888;
  if (argc > 1) {
    port = static_cast<uint16_t>(std::atoi(argv[1]));
  }
  uint16_t rtmpPort = 1935;
  if (argc > 2) {
    rtmpPort = static_cast<uint16_t>(std::atoi(argv[2]));
  }

  EventLoop loop;
  api::Router router;
  auth::AuthService authService;
  auth::RtmpTicketManager rtmpTicketManager;
  constexpr std::string_view kRtmpHost = "127.0.0.1";
  constexpr std::string_view kRtmpApp = "live";

  router.get("/api/health", [](const HttpRequest&, HttpResponse* response) {
    setJson(response, HttpResponse::k200Ok, "OK",
            api::makeOk("{\"service\":\"login_server\"}"));
  });

  router.post("/api/login", [&authService](const HttpRequest& request,
                                           HttpResponse* response) {
    auto body = api::parseJsonObject(request.body());
    if (!body) {
      setJson(response, HttpResponse::k400BadRequest, "Bad Request",
              api::makeError(1000, "bad request"));
      return;
    }

    auto username = body->getString("username");
    auto password = body->getString("password");
    auto roleText = body->getString("role");
    if (!username || !password || !roleText) {
      setJson(response, HttpResponse::k400BadRequest, "Bad Request",
              api::makeError(1000, "bad request"));
      return;
    }

    auth::UserRole role = auth::roleFromString(*roleText);
    auth::AuthResult result = authService.login(*username, *password, role);
    if (result.error != auth::AuthError::kOk) {
      setJson(response, HttpResponse::k200Ok, "OK",
              api::makeError(authErrorCode(result.error),
                             auth::authErrorMessage(result.error)));
      return;
    }

    setJson(response, HttpResponse::k200Ok, "OK",
            api::makeOk(makeLoginData(result.session)));
  });

  router.get("/api/me", [&authService](const HttpRequest& request,
                                       HttpResponse* response) {
    auto token = getBearerToken(request);
    if (!token) {
      setUnauthorized(response);
      return;
    }

    auto session = authService.verifyToken(*token);
    if (!session) {
      setUnauthorized(response);
      return;
    }

    setJson(response, HttpResponse::k200Ok, "OK",
            api::makeOk(makeUserData(*session)));
  });

  router.post("/api/logout", [&authService](const HttpRequest& request,
                                            HttpResponse* response) {
    auto token = getBearerToken(request);
    if (!token) {
      setUnauthorized(response);
      return;
    }

    if (!authService.logout(*token)) {
      setUnauthorized(response);
      return;
    }

    setJson(response, HttpResponse::k200Ok, "OK", api::makeOk());
  });

  router.post("/api/rtmp/ticket",
              [&authService, &rtmpTicketManager, kRtmpHost, rtmpPort, kRtmpApp](
                  const HttpRequest& request, HttpResponse* response) {
                auto token = getBearerToken(request);
                if (!token) {
                  setUnauthorized(response);
                  return;
                }

                auto session = authService.verifyToken(*token);
                if (!session) {
                  setUnauthorized(response);
                  return;
                }

                auto body = api::parseJsonObject(request.body());
                if (!body) {
                  setJson(response, HttpResponse::k400BadRequest, "Bad Request",
                          api::makeError(1000, "bad request"));
                  return;
                }

                auto streamName = body->getString("streamName");
                auto permissionText = body->getString("permission");
                if (!streamName || !permissionText ||
                    !isValidStreamName(*streamName)) {
                  setJson(response, HttpResponse::k400BadRequest, "Bad Request",
                          api::makeError(1000, "bad request"));
                  return;
                }

                auth::RtmpPermission permission =
                    auth::rtmpPermissionFromString(*permissionText);
                if (permission == auth::RtmpPermission::kUnknown) {
                  setJson(response, HttpResponse::k400BadRequest, "Bad Request",
                          api::makeError(1000, "bad request"));
                  return;
                }

                std::string streamKey = makeStreamKey(kRtmpApp, *streamName);
                auth::RtmpTicket ticket = rtmpTicketManager.createTicket(
                    *session, streamKey, permission);
                setJson(response, HttpResponse::k200Ok, "OK",
                        api::makeOk(makeRtmpTicketData(
                            ticket, kRtmpHost, rtmpPort, kRtmpApp,
                            *streamName)));
              });

  HttpServer server(&loop, InetAddress(port), "login_server");
  server.setHttpCallback(
      [&router](const HttpRequest& request, HttpResponse* response) {
        router.route(request, response);
      });

  rmuduo::rtmp::RtmpServer rtmpServer(&loop, InetAddress(rtmpPort),
                                      "teacher_backend_rtmp");
  rtmpServer.setAuthCallback(
      [&rtmpTicketManager](const std::string& streamKey,
                           const std::string& ticket, bool publish) {
        if (publish) {
          return rtmpTicketManager.verifyPublish(ticket, streamKey).has_value();
        }
        return rtmpTicketManager.verifyPlay(ticket, streamKey).has_value();
      });

  server.start();
  rtmpServer.start();
  loop.loop();
}
