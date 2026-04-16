# RTMP Rebuild Plan

## 目标

当前 `rtmp/` 目录里的代码更像早期草稿，不建议继续在原有结构上修补。更稳妥的做法是保留必要的协议笔记，然后基于现有 `net` 框架重新搭一个 RTMP 模块。

第一版目标建议限制为：

- 支持 RTMP over TCP 长连接
- 支持标准握手
- 支持 AMF0 command
- 支持一个 publisher，多个 player
- 支持 metadata / audio / video 转发

第一版暂时不做：

- 转码
- HLS / HTTP-FLV 输出
- 鉴权
- 录制
- 集群
- 回源

这样可以先做出一个结构清晰、可验证、可演进的最小闭环。

## 总体架构

建议按下面几层拆分：

1. `RtmpServer`
2. `RtmpConnection`
3. `RtmpSession` / `RtmpSessionManager`
4. `Chunk Codec` / `Message Assembler`
5. `AMF0 Codec`
6. `Command Handler`

职责划分如下：

- `RtmpServer`
  - 基于 `TcpServer` 接受连接
  - 给每个 `TcpConnection` 绑定 RTMP 上下文
  - 只负责接入，不负责协议细节

- `RtmpConnection`
  - 每个 TCP 连接一个实例
  - 负责连接级状态机
  - 负责推进握手、chunk 解码、message 分发
  - 不负责全局流管理

- `RtmpSession`
  - 表示一条流会话
  - 管理一个 publisher 和多个 player
  - 保存 metadata、sequence header、GOP 缓存
  - 不直接操作 socket

- `RtmpSessionManager`
  - 以 `app + streamName` 为 key 管理 session
  - 负责查找、创建、销毁 session

- `RtmpChunkCodec`
  - 负责从 TCP 字节流里解析 RTMP chunk
  - 处理 basic header、message header、extended timestamp
  - 处理半包、粘包、跨多次读取

- `RtmpMessageAssembler`
  - 负责把多个 chunk fragment 重组成完整 RTMP message
  - 维护 chunk stream 级上下文

- `Amf0`
  - 负责 command message 的编解码

- `RtmpCommandHandler`
  - 负责 `connect`、`createStream`、`publish`、`play` 等命令
  - 基于完整 RTMP message 工作

## 建议目录结构

```text
rtmp/
  RTMP_PLAN.md
  protocol/
    RtmpConstants.h
  amf/
    Amf0.h
    Amf0.cc
  chunk/
    RtmpChunkCodec.h
    RtmpChunkCodec.cc
  message/
    RtmpMessage.h
    RtmpMessageAssembler.h
    RtmpMessageAssembler.cc
  command/
    RtmpCommandHandler.h
    RtmpCommandHandler.cc
  session/
    RtmpSession.h
    RtmpSession.cc
    RtmpSessionManager.h
    RtmpSessionManager.cc
  server/
    RtmpServer.h
    RtmpServer.cc
    RtmpConnection.h
    RtmpConnection.cc
    RtmpHandshake.h
    RtmpHandshake.cc
```

如果暂时不想拆这么细，也至少要保证以下边界：

- 握手和 chunk 解码分开
- chunk 解码和 command 处理分开
- connection 和 session 分开
- AMF 编解码单独成模块

## Todo List

1. 清理现有 `rtmp` 草稿代码
2. 明确第一阶段功能边界
3. 重建 RTMP 模块骨架
4. 打通连接生命周期
5. 实现握手状态机
6. 实现 chunk 基础解码器
7. 实现 message 重组层
8. 实现 AMF0 编解码
9. 实现 command 分发器
10. 打通 `connect`
11. 打通 `createStream`
12. 打通 `publish`
13. 实现 `RtmpSessionManager`
14. 接收 metadata / audio / video
15. 打通 `play`
16. 加入 metadata 和 GOP 缓存
17. 实现断连清理与资源回收
18. 补测试与抓包验证
19. 补日志、指标和排障信息
20. 再评估高级能力

## 每一步的详细说明

### 1. 清理现有 `rtmp` 草稿代码

目标是把当前 `rtmp/` 从“未成型实验代码”变成“可重新设计的干净入口”。

建议做法：

- 删除当前未稳定的实现文件
- 保留协议笔记
- 保留一个空的 `CMakeLists.txt` 或暂时不接入顶层构建

原因：

- 现在的草稿没有形成清晰抽象
- 如果继续修补，协议解析、连接状态、业务会话很容易耦合到一起
- 早期一次重建的成本，远小于后期反复返工

### 2. 明确第一阶段功能边界

这一步不是文档工作，而是为了避免后续开发失控。

必须先定清楚：

- 第一版只做 RTMP relay，不做转码
- 只做单机内存分发
- 只支持 AMF0
- 只支持最基本命令集
- 只支持单 publisher，多 player

如果这一步不先锁定，后面很容易边写边加功能，导致整体结构越来越散。

### 3. 重建 RTMP 模块骨架

先把文件结构和核心类型建出来，即使内部大多还是空实现也没关系。

这一步的目标是先把“模块边界”定下来，而不是立刻做协议细节。建议先定义：

- `RtmpServer`
- `RtmpConnection`
- `RtmpConnectionContext`
- `RtmpSession`
- `RtmpSessionManager`
- `RtmpChunkCodec`
- `RtmpMessageAssembler`
- `Amf0`
- `RtmpCommandHandler`

为什么要先做骨架：

- 先分层，再填逻辑
- 防止所有代码都堆到一个类里
- 便于后续单独测试每一层

### 4. 打通连接生命周期

这一阶段先不解析 RTMP，只验证接入链路。

要做到：

- 新连接进入时创建 RTMP 上下文
- 收到数据时进入 RTMP 入口函数
- 连接关闭时正确回收连接级状态
- 打出清晰日志

建议日志至少包括：

- 连接建立
- 当前连接 id
- 对端地址
- 收到字节数
- 连接关闭原因

这一步成功后，说明现有 `TcpServer` 这层可以平稳承载 RTMP。

### 5. 实现握手状态机

握手必须独立成一个连接级状态机，不要直接塞进 chunk 解码流程。

建议状态：

- `kWaitC0C1`
- `kSendS0S1S2`
- `kWaitC2`
- `kHandshakeDone`

实现重点：

- 支持增量输入
- 支持半包
- 支持一次 `onMessage` 收到多段数据
- 状态切换后继续消费剩余缓冲区

不要写成“假设一次收到完整 `C0C1`”的实现。网络层不会保证这一点。

阶段完成标准：

- 常见 RTMP 客户端能顺利完成握手
- 抓包能看到正确的 `C0/C1/S0/S1/S2/C2` 交换

### 6. 实现 chunk 基础解码器

握手完成后，所有 RTMP 业务数据都会走 chunk 层。

`RtmpChunkCodec` 要处理：

- basic header
- fmt
- chunk stream id
- message header
- extended timestamp
- inbound chunk size

这一步只负责“从字节流中切出 chunk 片段”，不负责上层命令语义。

实现关键点：

- 保留每个 chunk stream 的解析上下文
- 正确处理 format 0/1/2/3
- 处理跨多次读取的 payload

这一层是 RTMP 最容易出错的地方之一，建议单独做大量单元测试。

### 7. 实现 message 重组层

RTMP 的完整 message 往往会被拆成多个 chunk，所以必须有 assembler。

`RtmpMessageAssembler` 的职责：

- 按 chunk stream 维护正在组装的 message
- 在 payload 收满时输出完整 `RtmpMessage`
- 向上层隐藏 chunk 分片细节

这一层完成后，上层模块就不需要关心：

- 包有没有收全
- 一个 message 被拆成了几片
- 当前这片是不是最后一片

这能显著降低 command 层和媒体层的复杂度。

### 8. 实现 AMF0 编解码

命令消息的 payload 需要 AMF0 解码。

第一版建议只实现最小必要类型：

- Number
- Boolean
- String
- Null
- Object
- ECMA Array

这已经足够支撑：

- `connect`
- `createStream`
- `publish`
- `play`
- `_result`
- `onStatus`

建议把 AMF 做成独立 API，例如：

- 输入 `Buffer` 或字节序列，输出 `AmfValue` 列表
- 输入 `AmfValue` 列表，输出编码后的 payload

这样后续 command 层只关心语义，不关心二进制细节。

### 9. 实现 command 分发器

当收到了完整 command message 后，交给 `RtmpCommandHandler`。

它负责：

- 解码命令名
- 读取 transaction id
- 提取 command object 和参数
- 分发到具体 handler

建议先支持：

- `connect`
- `createStream`
- `publish`
- `play`
- `deleteStream`
- `closeStream`

这一步要刻意避免把命令处理写死在 `RtmpConnection` 里，否则连接类会迅速膨胀。

### 10. 打通 `connect`

这是第一个完整里程碑。

实现内容：

- 正确解析客户端 `connect`
- 返回必要控制消息
- 返回 `_result`

通常至少包括：

- Window Acknowledgement Size
- Set Peer Bandwidth
- Set Chunk Size
- `_result`

如果 `connect` 能成功，基本说明以下链路都打通了：

- 握手
- chunk 解码
- message 重组
- AMF 解码
- 基础响应编码

### 11. 打通 `createStream`

这一步主要是建立连接内的 stream 语义。

需要做的事：

- 接收 `createStream`
- 分配 stream id
- 返回 `_result`

设计注意点：

- 不要假设一个连接只会用一个 stream id
- 至少要为未来多 stream 保留映射结构

### 12. 打通 `publish`

收到 `publish` 后，说明客户端要开始推流。

需要做的事：

- 解析 app、streamName
- 在 `RtmpSessionManager` 中找到或创建 session
- 将当前连接标记为 publisher
- 返回发布成功状态

建议在这个阶段就把“连接”和“会话”彻底分开：

- `RtmpConnection` 代表客户端连接
- `RtmpSession` 代表一条流

### 13. 实现 `RtmpSessionManager`

这一层是业务结构的核心。

建议职责：

- 以 `app + streamName` 为 key 查找 session
- 创建新 session
- 绑定 publisher
- 添加和移除 player
- session 空闲时销毁

这层不要处理协议解析，不要依赖 socket 细节。  
它应该只表达流关系和转发语义。

### 14. 接收 metadata / audio / video

从 `publish` 开始，服务端会收到真正的媒体数据。

这个阶段先做最小能力：

- 收到 metadata 时保存最新版本
- 收到 audio message 时转发给 player
- 收到 video message 时转发给 player

此时先不追求首屏体验，只先验证：

- 推流端持续发送
- 服务端持续收包
- 播放端能收到并消费

### 15. 打通 `play`

当客户端发送 `play` 后，需要：

- 根据流名找到 session
- 把当前连接注册为 player
- 返回播放开始状态
- 开始发送已有缓存和实时媒体消息

这一步完成后，系统已经具备最小 RTMP relay 能力。

### 16. 加入 metadata 和 GOP 缓存

这是提升可用性的关键步骤。

建议至少缓存：

- 最新 metadata
- 最新音频 sequence header
- 最新视频 sequence header
- 最近一个 GOP

原因：

- 新播放器加入时，如果没有缓存，常常要等下一个关键帧才能出画
- 有了缓存后，新 player 可以更快起播

### 17. 实现断连清理与资源回收

这一步必须专门处理，不能顺手带过。

至少要覆盖：

- publisher 断开后 session 如何变化
- player 断开后如何从 session 移除
- 空 session 是否延迟销毁
- 跨线程移除时如何避免悬垂引用
- 连接关闭后回调链是否仍可能访问旧对象

这部分如果做不好，后面最常见的问题就是：

- 内存泄漏
- use-after-free
- session 状态脏掉

### 18. 补测试与抓包验证

RTMP 协议实现必须配合验证，不要靠“能跑起来”判断正确性。

建议至少有三类验证：

- 单元测试
  - AMF0 编解码
  - chunk header 解析
  - message 重组

- 集成测试
  - 握手
  - `connect`
  - `publish`
  - `play`

- 抓包验证
  - 用 Wireshark 或客户端日志确认时序和字段

抓包的价值非常高，因为很多 RTMP 问题不会直接崩溃，只会表现为客户端无响应或静默断开。

### 19. 补日志、指标和排障信息

RTMP 服务一旦开始跑真实流量，没有观测能力很难维护。

最低建议：

- 连接建立与断开日志
- 握手状态推进日志
- command 收发日志
- session 创建与销毁日志
- publisher/player 绑定关系日志
- 异常断流原因

进一步可以加：

- 当前连接数
- 当前 session 数
- 每路流码率
- 每个 player 的消费延迟

### 20. 再评估高级能力

只有在前面的最小闭环稳定后，再考虑以下能力：

- 鉴权
- 录制
- HLS / HTTP-FLV 输出
- 回源
- 集群
- 低延迟优化
- 更完整的 RTMP 兼容性

这些都不应该进入第一阶段。

## 推荐里程碑

### 阶段一：协议底座

- 清理旧代码
- 建立新骨架
- 打通连接生命周期
- 完成握手

完成标准：

- 客户端可以连入并完成 RTMP 握手

### 阶段二：控制面

- 完成 chunk 解码
- 完成 message 重组
- 完成 AMF0
- 打通 `connect`
- 打通 `createStream`

完成标准：

- 客户端可以成功建立 RTMP 控制会话

### 阶段三：推流

- 打通 `publish`
- 引入 `RtmpSessionManager`
- 接收 metadata/audio/video

完成标准：

- 服务端可以稳定接收一路推流

### 阶段四：播放

- 打通 `play`
- 实现消息广播
- 加入 metadata / GOP 缓存

完成标准：

- 一个 publisher，多个 player 可以正常播放

### 阶段五：工程化

- 断连清理
- 单元测试
- 集成测试
- 抓包验证
- 日志和指标

完成标准：

- 从“能演示”变成“可持续维护”

## 实施建议

实现顺序上，建议始终坚持这条原则：

- 先把连接状态跑通
- 再把协议状态跑通
- 再把命令语义跑通
- 最后再把媒体分发做完整

不要从 `publish/play` 倒着推协议，也不要一开始就追求支持所有 RTMP 细节。  
基于当前项目，最稳的路线是把 `net` 层复用到底，再把 RTMP 作为一个严格分层的上层协议重新实现。
