# RTMP Build Plan

## 目标

这份计划不再按“最小 RTMP 协议练手版”来写，而是按 `/home/ranx/work/viewcode/rtmp` 那个项目已经达到的复杂度来定目标。

对标目标不是完整流媒体平台，而是一个**单机、内存转发型 RTMP/HTTP-FLV 服务**，能力大致包括：

- 支持 RTMP 服务端接入
- 支持 `connect / createStream / publish / play / deleteStream`
- 支持 H.264 / AAC 转发
- 支持 metadata、sequence header 处理
- 支持 GOP 缓存
- 支持一个 publisher、多 player
- 支持 HTTP-FLV 作为旁路输出
- 具备基本事件通知和会话管理

当前仓库已经做到的是：

- `TcpServer` 接入链路完成
- RTMP 握手完成
- RTMP chunk/message 解析完成
- AMF0 基础解码完成
- `connect` 最小响应完成

所以接下来的计划，不是继续“从零设计”，而是要把现在这套实现推进到接近外部项目的架构深度。

## 外部项目的真实复杂度

`/home/ranx/work/viewcode/rtmp` 不是一个“只有 RTMP 握手和 command 的 demo”，它已经是一个小型流媒体服务器原型，至少包含 4 层能力：

### 1. 接入层

- `RtmpServer`
- `HttpFlvServer`
- 连接生命周期管理
- 事件回调

这一层负责：

- 接受连接
- 创建协议连接对象
- 路由到 RTMP 或 HTTP-FLV
- 对外报告流事件

### 2. 协议层

- `RtmpHandshake`
- `RtmpChunk`
- `amf`
- `RtmpConnection`

这一层负责：

- 握手
- chunk 编解码
- AMF 命令解析
- RTMP 命令收发

### 3. 会话层

- `RtmpSession`
- publisher/player 管理
- stream path 管理

这一层负责：

- 管理一路流
- 管理一个推流端和多个播放端
- 会话销毁

### 4. 媒体分发层

- metadata 保存与下发
- AVC/AAC sequence header
- GOP 缓存
- 音视频分发
- HTTP-FLV 复用

这一层负责：

- 让新 player 加入后快速起播
- 让 RTMP 和 HTTP-FLV 共用同一份媒体缓存

所以如果要按这个复杂度推进，当前仓库不能只停留在“协议类堆在 `RtmpServer` 里”，而要显式分层。

## 当前实现和目标实现的差距

当前实现的优点：

- 连接级状态抽象是对的
- `RtmpChunkParser` 和握手分开了
- `Command Message AMF0` 已经能解出结构化命令
- `connect` 已经能返回最小控制消息

当前实现的主要缺口：

### 1. 没有 `RtmpConnection`

现在大量协议逻辑还在 `RtmpServer::onMessage()` 里。  
这不适合继续扩展，因为：

- `createStream`
- `publish`
- `play`
- 媒体消息处理
- 状态回包

都属于**连接级协议行为**，不应该继续塞进 `RtmpServer`。

### 2. 会话层太薄

现在的 `RtmpSession / RtmpSessionManager` 只够表达“publisher/player 关系”，还没有：

- metadata
- sequence header
- GOP cache
- 新播放端首包补发
- 空 session 清理策略

### 3. 出站编码还只是最小版

现在有：

- `Amf0Encoder`
- `RtmpChunkWriter`

但还缺：

- 常用控制消息的统一封装
- `onStatus`
- `_result(createStream)`
- `publish/play` 响应

### 4. 媒体层还没接入

当前没有开始处理：

- RTMP video message
- RTMP audio message
- metadata `@setDataFrame/onMetaData`
- H.264/AAC sequence header

### 5. 没有 HTTP-FLV 旁路

外部项目的复杂度里，HTTP-FLV 不是附属功能，而是复用同一份 session 的第二协议出口。

## 新的目标架构

基于当前仓库，建议调整成下面这个结构：

```text
rtmp/
  RTMP_PLAN.md
  RTMP_MESSAGE_EXPLAIN.md

  RtmpTypes.h
  RtmpMessage.h

  Amf0Value.h/.cc
  Amf0Decoder.h/.cc
  Amf0Encoder.h/.cc

  RtmpHandshake.h/.cc
  RtmpChunkParser.h/.cc
  RtmpChunkWriter.h/.cc
  RtmpCommandMessage.h/.cc

  RtmpConnectionContext.h/.cc
  RtmpConnection.h/.cc

  RtmpSession.h/.cc
  RtmpSessionManager.h/.cc

  RtmpServer.h/.cc
```

如果继续扩：

```text
httpflv/
  HttpFlvServer.h/.cc
  HttpFlvConnection.h/.cc
```

关键边界要改成：

- `RtmpServer`
  - 只负责接入
  - 创建/绑定 `RtmpConnection`
  - 持有 `RtmpSessionManager`

- `RtmpConnection`
  - 负责所有连接级 RTMP 协议流程
  - 握手、chunk、AMF、command、媒体消息
  - 持有对 `RtmpServer` / `RtmpSessionManager` 的访问入口

- `RtmpSession`
  - 负责一路流的 publisher/player、缓存和分发

## 重写后的实施计划

下面的计划按“接近外部项目复杂度”的顺序来写，不再是单纯协议练习顺序。

### 阶段 A：把协议逻辑从 `RtmpServer` 迁到 `RtmpConnection`

这是第一优先级。

#### A1. 新增 `RtmpConnection`

职责：

- 持有 `TcpConnectionPtr`
- 持有或访问 `RtmpConnectionContext`
- 提供：
  - `onMessage(Buffer*)`
  - `handleHandshake()`
  - `handleMessages(std::vector<RtmpMessage>)`
  - `handleCommand(const RtmpCommandMessage&)`

为什么必须先做：

- 现在 `RtmpServer` 已经开始承载 command 行为了
- 再往下做 `createStream/publish/play` 会导致 server 膨胀

#### A2. `RtmpServer` 退化成接入层

改法：

- `onConnection()` 时给 `TcpConnection` 绑定 `RtmpConnection`
- `onMessage()` 里只转发给 `RtmpConnection`
- `RtmpServer` 只保留：
  - `RtmpSessionManager`
  - event callback
  - 配置项

### 阶段 B：补齐命令控制面

这部分做完后，协议层就接近外部项目的 command 复杂度。

#### B1. 处理 `createStream`

目标：

- 接收 `createStream`
- 返回 `_result`
- 分配一个 `stream id`

当前需要的修改：

- `RtmpConnectionContext` 新增：
  - `streamId`
  - `transaction counter` 或当前连接流状态

#### B2. 处理 `publish`

目标：

- 从 arguments 里拿到 `streamName`
- 组合出 `streamPath`，如 `/live/01`
- 到 `RtmpSessionManager` 里 `getOrCreate`
- 把当前连接设成 publisher
- 回 `onStatus(NetStream.Publish.Start)`

当前需要的修改：

- `RtmpConnectionContext` 新增：
  - `streamPath`
- `RtmpSession` 新增：
  - `setPublisher`
  - publisher 状态检查

#### B3. 处理 `play`

目标：

- 找到 session
- 把当前连接加成 player
- 回 `onStatus(NetStream.Play.Start)`
- 先发 metadata / sequence header / GOP，再发实时流

当前需要的修改：

- `RtmpSession` 要具备“新 player 入场补发缓存”的能力

#### B4. 处理 `deleteStream`

目标：

- 连接主动关闭发布或播放时，能从 session 中摘掉
- 空 session 可销毁

### 阶段 C：接入媒体消息

这是从“控制通了”到“真的能转发流”的分水岭。

#### C1. 处理 metadata

重点消息：

- `Data Message AMF0`
- 常见形式：`@setDataFrame`, `onMetaData`

要做的事：

- 解析 payload
- 把 metadata 存到 session
- 给已存在 player 广播 metadata

当前需要的修改：

- 需要一个 `metadata` 的结构化表示
- 或至少保存原始 AMF0 payload

#### C2. 处理 video

最小目标：

- 收到 RTMP video message 后分发给 session
- 识别 H.264 sequence header
- 识别关键帧

当前需要的修改：

- `RtmpSession` 新增：
  - latest AVC sequence header
  - video 分发接口

#### C3. 处理 audio

最小目标：

- 收到 RTMP audio message 后分发给 session
- 识别 AAC sequence header

当前需要的修改：

- `RtmpSession` 新增：
  - latest AAC sequence header
  - audio 分发接口

### 阶段 D：补 session 缓存层

这是对齐外部项目复杂度最关键的一步。

#### D1. metadata 缓存

- session 内持有 latest metadata
- 新 player 加入时先发 metadata

#### D2. sequence header 缓存

- session 内持有 AVC/AAC sequence header
- 新 player 加入时先发 sequence header

#### D3. GOP 缓存

- session 内保存最近 1 个或 N 个 GOP
- 新 player 加入时补发

当前建议：

- 先做“只缓存最近一个 GOP”
- 不要一开始就做复杂淘汰策略

### 阶段 E：事件和管理层补齐

这部分是为了对齐外部项目在“可运维性”和“服务化”上的复杂度。

#### E1. 事件回调

建议 `RtmpServer` 增加事件接口：

- `on_publish(streamPath)`
- `on_play(streamPath)`
- `on_unpublish(streamPath)`
- `on_close(streamPath)`

#### E2. 定时清理空 session

建议和外部项目一样，由 server 定时清空没有 publisher/player 的 session。

#### E3. 连接关闭后的角色清理

这里你已经有雏形，但后面必须和：

- publisher
- player
- stream id
- session

做完整收口。

### 阶段 F：HTTP-FLV 旁路

如果目标是接近外部项目复杂度，这一步不能省。

建议策略：

- 不要把 HTTP-FLV 当成 RTMP 的一部分写进 `rtmp/`
- 单独建 `httpflv/`
- 复用 `RtmpSession` 的 metadata / sequence header / GOP / AV frame 分发

这样结构会更清楚。

## 如何修改现在的代码

这一部分只说“从当前仓库出发，最应该怎么改”，不讲泛泛方案。

### 修改 1：先引入 `RtmpConnection`

当前问题：

- `RtmpServer.cc` 已经在做：
  - 握手推进
  - chunk 解析
  - command 解码
  - `connect` 响应

这条路继续走下去会很快失控。

建议改法：

- 新增 `RtmpConnection`
- `RtmpServer::onMessage()` 改成：
  - 取 context
  - 取 connection handler
  - `handler->onMessage(buf)`

这样后面 `createStream/publish/play/audio/video` 都进 `RtmpConnection`

### 修改 2：把出站消息封成统一 helper

当前 `RtmpServer` 里已经有：

- `sendWindowAcknowledgementSize`
- `sendSetPeerBandwidth`
- `sendSetChunkSize`
- `sendConnectSuccess`

继续扩下去会很散。

建议改法：

- 抽一个 `RtmpProtocolWriter` 或至少在 `RtmpConnection` 内聚：
  - `sendControlMessage(type, payload)`
  - `sendCommandResult(...)`
  - `sendOnStatus(...)`

### 修改 3：扩展 `RtmpConnectionContext`

当前已经有：

- `app`
- `tcUrl`
- `objectEncoding`
- chunk size / bandwidth

接下来至少还要加：

- `streamId`
- `streamName`
- `streamPath`
- 连接模式：
  - unknown
  - publisher
  - player
- 播放/发布是否已完成

### 修改 4：让 `RtmpSession` 从“关系对象”变成“媒体会话对象”

当前 `RtmpSession` 还比较轻。

接下来要逐步扩成：

- publisher
- players
- metadata
- AVC sequence header
- AAC sequence header
- GOP cache
- sendToPlayers()
- replayCacheToPlayer()

### 修改 5：不要再往 `RtmpServer` 加媒体逻辑

后面这些都不要写进 `RtmpServer`：

- video/audio 消息处理
- metadata 解析
- session 缓存更新
- player 首包补发

这些都应该放在：

- `RtmpConnection`
- `RtmpSession`

### 修改 6：现在的 AMF0 结构足够继续，但后面要补编码场景

当前 `Amf0Decoder/Encoder` 已够支撑：

- `connect`
- `_result`

接下来还要补：

- `createStream` 的 `_result`
- `onStatus`
- metadata 透传或重编码

所以这一层先不必重构，优先继续向上推功能。

## 新的推荐开发顺序

如果从今天开始继续开发，我建议顺序改成：

1. 新增 `RtmpConnection`，把 `RtmpServer` 瘦身
2. 实现 `createStream`
3. 实现 `publish`
4. 实现 `play`
5. 处理 metadata/video/audio
6. 给 `RtmpSession` 加 sequence header 和 GOP cache
7. 做 `deleteStream` 和断连回收
8. 再决定要不要加 HTTP-FLV

这个顺序比“继续直接在 `RtmpServer` 上叠命令”更稳。

## 一句话结论

如果目标只是“能讲 RTMP 协议”，当前结构已经够了。  
如果目标是接近 `/home/ranx/work/viewcode/rtmp` 的复杂度，那么下一阶段的关键不是继续补协议细节，而是**把当前 `RtmpServer` 里的协议逻辑迁到 `RtmpConnection`，再把 `RtmpSession` 扩成真正的媒体会话层**。
