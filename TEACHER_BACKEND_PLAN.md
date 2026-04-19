# 登录与 RTMP 授权服务计划书

## 1. 项目目标

本阶段完成助教后台系统的登录服务，并在登录后允许用户使用 RTMP 推流和拉流。

本计划不实现负载均衡，不实现多流媒体节点分配。RTMP 服务先按单机模式接入。

登录服务器需要提供：

- 老师登录。
- 学生登录。
- token 生成和校验。
- 查询当前登录用户。
- 登出。
- 基础权限区分。
- 登录后申请 RTMP 推流地址。
- 登录后申请 RTMP 拉流地址。
- RTMP `publish` 和 `play` 时做鉴权。

RTMP 使用目标：

- 老师可以推流。
- 老师可以拉流。
- 学生可以推流。
- 学生可以拉流。
- 所有推拉流都必须先登录。

当前项目已有基础：

- `net/`：基础 TCP 网络库。
- `http/`：HTTP server，已支持 GET 和 POST body。
- `rtmp/`：RTMP server，已具备 `connect/createStream/publish/play` 基础流程。
- `example/`：测试入口。

本阶段重点是把登录服务从 `example/Httpserver_test.cc` 中拆出来，形成正式业务模块。

## 2. 范围说明

本阶段要做：

- HTTP API 路由。
- JSON 请求解析。
- JSON 响应封装。
- 用户存储。
- 密码校验。
- token 生成、保存、过期和校验。
- 登录、登出、查询当前用户接口。
- RTMP 推拉流 ticket 生成。
- RTMP 推拉流 ticket 校验。
- RTMP `publish/play` 接入鉴权。

本阶段不做：

- 负载均衡。
- 多流媒体节点管理。
- 节点注册和心跳。
- HTTP-FLV、HLS、WebRTC。
- 完整数据库系统。

## 3. 推荐目录结构

建议新增以下目录：

```text
api/
  ApiServer.h
  ApiServer.cc
  Router.h
  Router.cc
  JsonResponse.h
  JsonRequest.h

auth/
  AuthService.h
  AuthService.cc
  UserStore.h
  UserStore.cc
  TokenManager.h
  TokenManager.cc
  RtmpTicketManager.h
  RtmpTicketManager.cc
  AuthTypes.h

server/
  TeacherBackendServer.h
  TeacherBackendServer.cc
```

职责边界：

- `http/`：只负责 HTTP 协议解析和响应编码。
- `api/`：负责路由、JSON 解析、统一响应、错误码。
- `auth/`：负责用户、密码、token、登录态和 RTMP ticket。
- `rtmp/`：负责 RTMP 握手、命令、媒体消息、推拉流会话。
- `server/`：负责把 HTTP 登录服务和 RTMP 服务组装到同一个进程。
- `example/`：只保留测试示例，不承载正式业务逻辑。

## 4. 登录服务器架构

短期采用单进程 HTTP + RTMP 服务：

```text
teacher_backend
  - HttpServer :8888
  - RtmpServer :1935
  - Router
  - AuthService
  - UserStore
  - TokenManager
  - RtmpTicketManager
```

HTTP 登录流程：

```text
Client
  |
  v
HttpServer
  |
  v
Router
  |
  v
AuthService
  |
  +--> UserStore
  |
  +--> TokenManager
```

RTMP 使用流程：

```text
Client
  |
  | 1. POST /api/login
  v
HttpServer
  |
  | 2. 返回 login token
  v
Client
  |
  | 3. POST /api/rtmp/ticket
  |    Authorization: Bearer <login-token>
  v
AuthService + RtmpTicketManager
  |
  | 4. 返回短期 RTMP URL
  v
Client
  |
  | 5. ffmpeg/ffplay 使用 RTMP URL
  v
RtmpServer
  |
  | 6. publish/play 时校验 ticket
  v
RtmpSessionManager
```

## 5. 数据模型

用户角色：

```cpp
enum class UserRole {
  Teacher,
  Student,
};
```

用户信息：

```cpp
struct User {
  std::string userId;
  std::string username;
  std::string passwordHash;
  UserRole role;
  bool enabled;
};
```

登录态：

```cpp
struct AuthSession {
  std::string token;
  std::string userId;
  UserRole role;
  int64_t expireAt;
};
```

RTMP ticket：

```cpp
enum class RtmpPermission {
  Publish,
  Play,
};

struct RtmpTicket {
  std::string ticket;
  std::string userId;
  UserRole role;
  std::string streamKey;
  bool allowPublish;
  bool allowPlay;
  int64_t expireAt;
};
```

第一阶段可以使用内存用户表，预置测试用户：

```text
teacher01 / 123456 / teacher
student01 / 123456 / student
student02 / 123456 / student
```

后续再替换为 JSON 文件、SQLite 或 MySQL。

RTMP ticket 第一阶段也存在内存中，默认有效期建议为 5 分钟。ticket 只用于 RTMP 推拉流，不直接复用登录 token。

## 6. API 设计

### 6.1 登录

请求：

```http
POST /api/login
Content-Type: application/json

{
  "username": "teacher01",
  "password": "123456",
  "role": "teacher"
}
```

成功响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "token": "random_token",
    "userId": "teacher01",
    "username": "teacher01",
    "role": "teacher",
    "expireAt": 1710000000
  }
}
```

失败响应：

```json
{
  "code": 1001,
  "message": "invalid username or password",
  "data": null
}
```

### 6.2 查询当前用户

请求：

```http
GET /api/me
Authorization: Bearer random_token
```

响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "userId": "teacher01",
    "username": "teacher01",
    "role": "teacher"
  }
}
```

### 6.3 登出

请求：

```http
POST /api/logout
Authorization: Bearer random_token
```

响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": null
}
```

### 6.4 健康检查

请求：

```http
GET /api/health
```

响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "service": "login_server"
  }
}
```

### 6.5 申请 RTMP 推拉流地址

请求：

```http
POST /api/rtmp/ticket
Authorization: Bearer random_token
Content-Type: application/json

{
  "streamName": "class01_teacher01",
  "permission": "both"
}
```

`permission` 可选值：

```text
publish
play
both
```

因为当前需求是老师和学生都可以推拉流，所以第一阶段允许所有已登录用户申请 `publish`、`play` 或 `both`。

成功响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "streamKey": "/live/class01_teacher01",
    "publishUrl": "rtmp://127.0.0.1:1935/live/class01_teacher01?ticket=rtmp_ticket",
    "playUrl": "rtmp://127.0.0.1:1935/live/class01_teacher01?ticket=rtmp_ticket",
    "expireAt": 1710000000
  }
}
```

如果只申请推流：

```json
{
  "streamName": "student01_screen",
  "permission": "publish"
}
```

则只返回 `publishUrl`。

如果只申请拉流：

```json
{
  "streamName": "student01_screen",
  "permission": "play"
}
```

则只返回 `playUrl`。

## 7. 错误码

建议先定义一组基础错误码：

```text
0    ok
1000 bad request
1001 invalid username or password
1002 unauthorized
1003 forbidden
1004 token expired
1005 user disabled
1006 unsupported role
1100 invalid rtmp ticket
1101 rtmp ticket expired
1102 rtmp publish forbidden
1103 rtmp play forbidden
2000 internal error
```

HTTP 状态码建议：

- `200 OK`：业务成功或业务可预期失败，具体看 JSON `code`。
- `400 Bad Request`：JSON 格式错误、字段缺失。
- `401 Unauthorized`：没有 token、token 无效、token 过期。
- `403 Forbidden`：角色权限不足。
- `404 Not Found`：路由不存在。
- `500 Internal Server Error`：服务器内部错误。

## 8. 认证规则

token 放在 HTTP header：

```http
Authorization: Bearer <token>
```

校验流程：

1. 从 header 读取 `Authorization`。
2. 判断是否以 `Bearer ` 开头。
3. 提取 token。
4. 调用 `TokenManager::verify(token)`。
5. token 不存在或过期则返回 `401 Unauthorized`。
6. token 有效则得到 `userId` 和 `role`。

token 生命周期：

- 默认有效期：2 小时。
- 登录时生成新 token。
- 登出时删除 token。
- 查询接口命中 token 时可以选择是否刷新过期时间，第一阶段不刷新。

RTMP 不直接使用 HTTP header，所以 RTMP 鉴权使用 URL query 中的 ticket：

```text
rtmp://127.0.0.1:1935/live/student01_screen?ticket=<rtmp-ticket>
```

RTMP ticket 校验流程：

1. 客户端先登录，获得 login token。
2. 客户端调用 `/api/rtmp/ticket`，获得 RTMP URL。
3. RTMP 客户端连接 `RtmpServer`。
4. `connect` 阶段记录 `app`，例如 `live`。
5. `publish` 或 `play` 阶段解析 `streamName`。
6. 从 `streamName` 中拆出真实流名和 `ticket`。
7. 组合 `streamKey`，例如 `/live/student01_screen`。
8. 调用 `RtmpTicketManager::verify(ticket, streamKey, action)`。
9. 校验通过才允许 `publish` 或 `play`。
10. 校验失败则发送 RTMP `onStatus` 错误并关闭连接。

注意：登录 token 有效期可以较长，RTMP ticket 应较短。这样即使 RTMP URL 泄露，也能降低风险。

## 9. RTMP 授权设计

### 9.1 URL 设计

推流：

```bash
ffmpeg -re -i input.mp4 -c copy -f flv \
  "rtmp://127.0.0.1:1935/live/student01_screen?ticket=<rtmp-ticket>"
```

拉流：

```bash
ffplay "rtmp://127.0.0.1:1935/live/student01_screen?ticket=<rtmp-ticket>"
```

RTMP 中：

- `app` 是 `live`。
- `streamName` 是 `student01_screen?ticket=<rtmp-ticket>`。
- 真实流名是 `student01_screen`。
- `streamKey` 是 `/live/student01_screen`。

### 9.2 权限规则

第一阶段规则：

- 老师可以申请任意流的推流和拉流 ticket。
- 学生可以申请任意流的推流和拉流 ticket。
- 所有推拉流都必须带有效 ticket。
- 同一路流仍然只允许一个 publisher。
- 同一路流允许多个 player。

后续可以收紧权限：

- 学生只能推自己的屏幕流。
- 学生只能拉老师允许的流。
- 老师可以拉本班学生流。
- 老师可以推课堂讲解流。

### 9.3 RTMP 接入点

需要在 `rtmp/RtmpConnection.cc` 中接入鉴权：

- `handlePublish()`：在 `session->setPublisher(conn)` 之前校验 publish ticket。
- `handlePlay()`：在 `session->addPlayer(conn)` 之前校验 play ticket。

建议新增工具函数：

```cpp
struct RtmpStreamTarget {
  std::string streamName;
  std::string ticket;
};

std::optional<RtmpStreamTarget> parseStreamTarget(std::string_view streamName);
```

`publish/play` 收到的原始 `streamName` 可能是：

```text
student01_screen?ticket=abc
```

解析后：

```text
streamName = student01_screen
ticket = abc
```

然后把 `context->setStreamName()` 设置为真实流名，不把 query 放进 `streamKey`。

## 10. 密码处理

第一阶段为了快速打通，可以先用明文密码或简单 hash。

正式版本建议：

- 每个用户生成独立 salt。
- 使用 SHA-256 + salt 作为最低实现。
- 更推荐 bcrypt 或 argon2。
- 不在日志中打印明文密码。
- 登录失败只返回统一错误，不区分用户名不存在或密码错误。

## 11. 线程安全

如果 `HttpServer` 开启多线程，以下模块需要线程安全：

- `UserStore`
- `TokenManager`
- `RtmpTicketManager`
- `AuthService`

第一阶段可以用 `std::mutex` 保护内部容器：

```cpp
std::mutex mutex_;
std::unordered_map<std::string, AuthSession> sessions_;
```

注意不要在持锁状态下做耗时操作。

## 12. 开发步骤

### 阶段 1：API 基础设施

- 新增 `api/Router.h/.cc`。
- 支持 `GET`、`POST` 路由注册。
- 新增 `api/JsonResponse.h`。
- 统一生成 JSON 响应。
- 引入或封装 JSON 解析工具。

### 阶段 2：认证核心

- 新增 `auth/AuthTypes.h`。
- 新增 `auth/UserStore.h/.cc`。
- 新增 `auth/TokenManager.h/.cc`。
- 新增 `auth/AuthService.h/.cc`。
- 预置老师和学生测试账号。

### 阶段 3：登录接口

- 实现 `POST /api/login`。
- 校验 JSON 字段。
- 校验用户名、密码和角色。
- 生成 token。
- 返回用户信息。

### 阶段 4：登录态接口

- 实现 `GET /api/me`。
- 实现 `POST /api/logout`。
- 实现 `GET /api/health`。
- 增加未登录、token 过期、角色错误测试。

### 阶段 5：测试和示例

- 增加 `example/login_server.cc`。
- 保留 `example/Httpserver_test.cc` 作为 HTTP 基础示例。
- 使用 `curl` 验证登录、查询、登出。
- 增加基础单元测试或最小可执行测试。

### 阶段 6：RTMP ticket 接口

- 新增 `auth/RtmpTicketManager.h/.cc`。
- 实现 RTMP ticket 生成、校验、过期删除。
- 实现 `POST /api/rtmp/ticket`。
- 返回 `publishUrl` 和 `playUrl`。

### 阶段 7：RTMP 鉴权接入

- 让正式服务同时启动 `HttpServer` 和 `RtmpServer`。
- 将 `RtmpTicketManager` 传入 RTMP 模块。
- 在 `publish` 前校验 publish 权限。
- 在 `play` 前校验 play 权限。
- 校验失败时返回 RTMP 错误状态并断开。

### 阶段 8：推拉流验证

- 登录获得 token。
- 申请 RTMP ticket。
- 使用 ffmpeg 推流。
- 使用 ffplay 拉流。
- 验证无 ticket、错误 ticket、过期 ticket 都不能推拉流。

## 13. 测试命令设计

启动服务：

```bash
./build/bin/login_server
```

健康检查：

```bash
curl -v http://127.0.0.1:8888/api/health
```

老师登录：

```bash
curl -v http://127.0.0.1:8888/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"teacher01","password":"123456","role":"teacher"}'
```

查询当前用户：

```bash
curl -v http://127.0.0.1:8888/api/me \
  -H "Authorization: Bearer <token>"
```

登出：

```bash
curl -v -X POST http://127.0.0.1:8888/api/logout \
  -H "Authorization: Bearer <token>"
```

登出后再次查询：

```bash
curl -v http://127.0.0.1:8888/api/me \
  -H "Authorization: Bearer <token>"
```

预期返回 `401 Unauthorized`。

申请 RTMP URL：

```bash
curl -v http://127.0.0.1:8888/api/rtmp/ticket \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"streamName":"student01_screen","permission":"both"}'
```

推流：

```bash
ffmpeg -re -stream_loop -1 -i input.mp4 -c copy -f flv \
  "rtmp://127.0.0.1:1935/live/student01_screen?ticket=<rtmp-ticket>"
```

拉流：

```bash
ffplay "rtmp://127.0.0.1:1935/live/student01_screen?ticket=<rtmp-ticket>"
```

未登录申请 RTMP URL：

```bash
curl -v http://127.0.0.1:8888/api/rtmp/ticket \
  -H "Content-Type: application/json" \
  -d '{"streamName":"student01_screen","permission":"both"}'
```

预期返回 `401 Unauthorized`。

无 ticket 推流：

```bash
ffmpeg -re -i input.mp4 -c copy -f flv \
  "rtmp://127.0.0.1:1935/live/student01_screen"
```

预期 RTMP 服务拒绝 publish。

## 14. 里程碑

### 第 1 天

- 完成 HTTP 路由器。
- 完成 JSON 响应封装。
- 完成 `/api/health`。

### 第 2 天

- 完成 `UserStore`。
- 完成 `TokenManager`。
- 完成 `AuthService`。

### 第 3 天

- 完成 `/api/login`。
- 完成 `/api/me`。
- 完成 `/api/logout`。
- 完成 curl 验证。

### 第 4 天

- 整理示例入口 `login_server`。
- 增加错误处理。
- 增加基础测试。
- 清理 `example/Httpserver_test.cc` 中不属于示例的业务逻辑。

### 第 5 天

- 新增 `RtmpTicketManager`。
- 实现 `/api/rtmp/ticket`。
- 返回推流和拉流 URL。

### 第 6 天

- 新增正式入口 `teacher_backend`。
- 同进程启动 HTTP 和 RTMP。
- RTMP `publish/play` 接入 ticket 校验。

### 第 7 天

- 完成 ffmpeg 推流测试。
- 完成 ffplay 拉流测试。
- 完成无 ticket、错误 ticket、过期 ticket 的拒绝测试。

## 15. 下一步建议

下一步先实现 `auth/RtmpTicketManager` 和 `/api/rtmp/ticket`。这一步完成后，HTTP 登录服务就能给已登录用户签发 RTMP URL；再下一步把 RTMP 的 `publish/play` 接入 ticket 校验。
