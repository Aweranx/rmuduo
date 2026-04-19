# 登录服务器计划书

## 1. 项目目标

本阶段只完成助教后台系统的登录服务器，不实现负载均衡，不实现流媒体节点分配。

登录服务器需要提供：

- 老师登录。
- 学生登录。
- token 生成和校验。
- 查询当前登录用户。
- 登出。
- 基础权限区分。
- 后续接入老师后台、学生端和 RTMP 服务时可复用的认证能力。

当前项目已有基础：

- `net/`：基础 TCP 网络库。
- `http/`：HTTP server，已支持 GET 和 POST body。
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

本阶段不做：

- 负载均衡。
- 多流媒体节点管理。
- 节点注册和心跳。
- 学生屏幕流分配。
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
  AuthTypes.h
```

职责边界：

- `http/`：只负责 HTTP 协议解析和响应编码。
- `api/`：负责路由、JSON 解析、统一响应、错误码。
- `auth/`：负责用户、密码、token 和登录态。
- `example/`：只保留测试示例，不承载正式业务逻辑。

## 4. 登录服务器架构

短期采用单进程 HTTP 服务：

```text
login_server
  - HttpServer :8888
  - Router
  - AuthService
  - UserStore
  - TokenManager
```

请求流程：

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

第一阶段可以使用内存用户表，预置测试用户：

```text
teacher01 / 123456 / teacher
student01 / 123456 / student
student02 / 123456 / student
```

后续再替换为 JSON 文件、SQLite 或 MySQL。

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

## 9. 密码处理

第一阶段为了快速打通，可以先用明文密码或简单 hash。

正式版本建议：

- 每个用户生成独立 salt。
- 使用 SHA-256 + salt 作为最低实现。
- 更推荐 bcrypt 或 argon2。
- 不在日志中打印明文密码。
- 登录失败只返回统一错误，不区分用户名不存在或密码错误。

## 10. 线程安全

如果 `HttpServer` 开启多线程，以下模块需要线程安全：

- `UserStore`
- `TokenManager`
- `AuthService`

第一阶段可以用 `std::mutex` 保护内部容器：

```cpp
std::mutex mutex_;
std::unordered_map<std::string, AuthSession> sessions_;
```

注意不要在持锁状态下做耗时操作。

## 11. 开发步骤

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

## 12. 测试命令设计

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

## 13. 里程碑

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

## 14. 下一步建议

下一步先实现 `api/Router` 和 `api/JsonResponse`。登录服务器的核心接口都依赖路由和统一响应格式，先做这层可以避免继续把业务分支写进 `Httpserver_test.cc`。
