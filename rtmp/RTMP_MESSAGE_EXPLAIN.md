# RTMP Message / Chunk 说明

这份文档只解释当前项目已经实现到的 RTMP `chunk -> message` 这一层。  
目标不是覆盖完整规范，而是帮助你把代码里的字段、状态和协议定义一一对应起来。

可对照阅读的代码：

- [RtmpChunkParser.h](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.h:15)
- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:34)
- [RtmpMessage.h](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpMessage.h:8)
- [RtmpServer.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpServer.cc:97)

## 1. RTMP 为什么要分 Chunk 和 Message

RTMP 在 TCP 之上并不是直接发送“完整业务消息”，而是先把业务消息切成一个个 `chunk` 再发。

原因是：

- 一个视频帧可能很大
- 控制消息通常很小
- 如果直接顺序发送完整大包，小消息会被阻塞

所以 RTMP 的做法是：

- 上层先定义一个完整的 `message`
- 再把 message 按 chunk size 拆成多个 `chunk`
- 多种消息可以在网络上交错传输

你可以把它理解为：

- `message` 是逻辑上的完整消息
- `chunk` 是网络传输时的分片

当前代码的对应关系：

- `RtmpMessage` 表示完整 message
- `RtmpChunkParser` 负责把多个 chunk 重新拼成 `RtmpMessage`

## 2. 一个 Chunk 由什么组成

一个 RTMP chunk 通常由三部分组成：

1. Basic Header
2. Message Header
3. Chunk Data

某些情况下还会多一个：

4. Extended Timestamp

顺序上通常是：

```text
+--------------+----------------+----------------------+------------+
| Basic Header | Message Header | Extended Timestamp ? | Chunk Data |
+--------------+----------------+----------------------+------------+
```

注意：

- `Extended Timestamp` 不是每次都有
- `Message Header` 长度也不是固定的
- 它的长度由 `fmt` 决定

## 3. Basic Header 是什么

Basic Header 的作用只有两个：

- 表明这块 chunk 的 header 格式类型，也就是 `fmt`
- 表明它属于哪个 `chunk stream`，也就是 `csid`

在代码里对应：

- `parseBasicHeader()`  
  见 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:112)

### 3.1 fmt 是什么

`fmt` 占 basic header 的高 2 位，它决定“后面 message header 还会带多少信息”。

它不是 message type，也不是业务命令类型。  
它只表示“这次 chunk 头相对上一次同一 `csid` 的 chunk，省略了多少字段”。

当前代码里定义在：

- [RtmpTypes.h](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpTypes.h:10)

对应关系：

- `fmt = 0`
- `fmt = 1`
- `fmt = 2`
- `fmt = 3`

下面是它们的含义。

### 3.2 fmt = 0

含义：完整头。

表示这次 chunk 带了完整的 message header，通常用于：

- 该 `csid` 上下文还没有建立时
- 流的开始或突变。 每个消息流的第一个 Chunk 必须使用 fmt=0；如果流的时间戳复位了，也必须使用它。

`fmt=0` 会提供：

- timestamp
- message length
- message type id
- message stream id

在代码里是：

- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:158)

### 3.3 fmt = 1

含义：省略 `message stream id`。
- 可变长度消息。 比如视频流，StreamID 与上一个相同，但每一帧的大小（Length）不一样。此时省略 StreamID。

表示：

- 这个 chunk 仍然属于同一个 `csid`
- `message stream id` 和上一次一样
- 但新的 message 可能有新的长度、类型和时间戳增量

`fmt=1` 会提供：

- timestamp delta
- message length
- message type id

不会提供：

- message stream id

在代码里是：

- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:196)

### 3.4 fmt = 2

含义：只提供 `timestamp delta`。
固定长度消息。 比如音频流，每包大小和类型都固定。此时只需记录时间戳增量，省略 Length、TypeID 和 StreamID。

表示：

- `message stream id` 复用上一次
- `message length` 复用上一次
- `message type id` 复用上一次
- 只有时间戳变化了

这种格式比 `fmt=1` 更省字节。

在代码里是：

- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:225)

### 3.5 fmt = 3

含义：不带新的 message header，完全复用上一次该 `csid` 的头信息。

它有两种常见场景：

1. 一个大 message 被切成多个 chunk  
   后续分片通常就用 `fmt=3`

2. 连续的新 message 恰好头字段都和上一次一样  
   也可以直接复用

在当前代码里：

- 如果前一个 message 还没收完，`fmt=3` 就是续片
- 如果前一个 message 已收完，`fmt=3` 就按“新 message 复用旧头”处理

对应代码：

- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:252)

## 4. csid 是什么

`csid` 是 `chunk stream id`，表示这块 chunk 属于哪条 chunk stream。
决定了 Chunk 在网络连接（Connection）上的“多路复用”方式。
CSID 将不同的数据流区分在不同的通道里。例如，音频数据走 CSID 4，视频数据走 CSID 5。接收端根据 CSID 将交织在一起的 Chunk 重新组装回完整的 Message。

注意它不是：

- socket id
- message stream id
- app/stream name

它只是 RTMP chunk 层的“复用通道编号”。

为什么需要它？

- 因为一个连接上可能同时穿插多种消息
- 每种消息的 chunk 分片状态要分别维护
- 所以解析器必须按 `csid` 维护上下文

当前代码就是这么做的：

- `chunkStreams_` 以 `csid` 为 key 保存状态  
  见 [RtmpChunkParser.h](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.h:47)

### 4.1 basic header 长度为什么不固定

因为 `csid` 的编码方式有 3 种：

- 1 字节 basic header：`csid >= 2 && csid <= 63`
- 2 字节 basic header：`csid = 64 ~ 319`
- 3 字节 basic header：更大的 `csid`

当前代码都支持了解析：

- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:121)

## 5. Message Header 里有什么

Message Header 不是固定长度，由 `fmt` 决定。

### 5.1 fmt=0 时，长度 11 字节

字段包括：

- `timestamp`，3 字节
- `message length`，3 字节
- `message type id`，1 字节
- `message stream id`，4 字节，小端

注意最后这个 `message stream id` 是 little-endian。  
代码里用的是：

- `ReadUint32LE(data + 7)`  
  见 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:166)

### 5.2 fmt=1 时，长度 7 字节

字段包括：

- `timestamp delta`，3 字节
- `message length`，3 字节
- `message type id`，1 字节

### 5.3 fmt=2 时，长度 3 字节

字段只有：

- `timestamp delta`，3 字节

### 5.4 fmt=3 时，长度 0 字节

没有新的 message header，直接复用历史状态。

## 6. Extended Timestamp 是什么

RTMP 普通 header 里的时间戳字段只有 3 字节，也就是最大 `0xFFFFFF`。

如果真实时间戳或时间戳增量超过这个值，就会：

- 先在 3 字节时间字段里写 `0xFFFFFF`
- 再额外跟 4 字节 `extended timestamp`

所以 `extended timestamp` 的出现条件是：

- `timestamp == 0xFFFFFF`
- 或 `timestamp delta == 0xFFFFFF`

当前代码里对应：

- `fmt=0` 的扩展时间戳处理  
  见 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:170)
- `fmt=1`  
  见 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:206)
- `fmt=2`  
  见 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:233)
- `fmt=3` 复用场景  
  见 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:260)

## 7. timestamp 和 timestamp delta 的区别

这两个字段容易混。

### 7.1 timestamp

绝对时间戳。  
通常用于 `fmt=0`。

含义是：

- 这个 message 的完整时间戳是多少

### 7.2 timestamp delta

相对时间戳。  
通常用于 `fmt=1` 和 `fmt=2`。

含义是：

- 当前消息时间戳相对于上一个消息增加了多少

所以在代码里：

- `fmt=0` 直接设置 `state->timestamp`
- `fmt=1/2` 会 `state->timestamp += state->timestampDelta`

对应代码：

- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:176)
- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:219)
- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:246)

## 8. message stream id 和 chunk stream id 有什么区别

这是 RTMP 最容易混的两个概念。

### 8.1 chunk stream id (`csid`)

作用：

- chunk 层复用
- 帮助解析器找到“这条 chunk stream 的历史上下文”

它主要服务于：

- `fmt` 头压缩
- chunk 分片重组

可以把它理解成：

- “这块传输分片属于哪条 chunk 通道”
- 它解决的是“网络上传输时怎么复用和压缩 header”

它的特点是：

- 同一个连接里可以有多个 `csid`
- 不同 `csid` 各自维护独立的 chunk 头上下文
- `fmt=1/2/3` 都依赖“同一个 `csid` 的上一条历史状态”

在当前代码里：

- 由 `parseBasicHeader()` 解析出来  
  见 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:112)
- 最终写进 `RtmpMessage::chunkStreamId`
- 并作为 `chunkStreams_` 的 key 来保存状态  
  见 [RtmpChunkParser.h](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.h:47)

### 8.2 message stream id
用来处理多路流，简单场景只考虑单路流
作用：

- 表示这条 message 属于哪个 RTMP 消息流
- 更接近 RTMP 命令/媒体层的语义

它不是 chunk 层的编号，而是上层 message 的逻辑标识。

可以把它理解成：

- “这条完整 RTMP 消息属于哪个业务流上下文”
- 它解决的是“上层命令/音视频消息属于谁”

它的特点是：

- 它出现在 `fmt=0` 的 11 字节 message header 里
- `fmt=1/2/3` 不会重复带它，而是沿用当前 `csid` 上次保存的值
- 它和 `csid` 没有一一对应关系，它们属于不同层次

在当前代码里：

- `fmt=0` 时通过 `ReadUint32LE(data + 7)` 解析出来  
  见 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:166)
- 保存在 `ChunkStreamState::messageStreamId`
- 产出完整消息时写进 `RtmpMessage::messageStreamId`

当前代码里两个字段分别是：

- `chunkStreamId`
- `messageStreamId`

见 [RtmpMessage.h](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpMessage.h:8)

## 9. 当前代码怎么从 Chunk 拼成 Message

这里再用一句话对比：

- `csid` 决定“我该去哪个 chunk 状态槽里继续拼”
- `message stream id` 决定“这个拼出来的完整 message 逻辑上属于哪个 RTMP 流”

也就是说：

- `csid` 是解析器内部很关心的字段
- `message stream id` 是后续命令层、媒体层更关心的字段

如果只记一个最小区别，可以记成：

- `csid` 面向传输分片
- `message stream id` 面向上层消息语义

当前解析流程是：

1. 先解析 basic header  
   拿到 `fmt` 和 `csid`

2. 再根据 `fmt` 解析 message header  
   更新该 `csid` 对应的状态

3. 计算当前 chunk 这次能携带多少 payload  
   `min(remaining, inChunkSize_)`

4. 拷贝 payload 到 `state.payload`

5. 如果 `bytesRead == messageLength`  
   说明完整 message 到齐，生成 `RtmpMessage`

对应代码入口：

- [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:51)

## 10. 当前项目里的 Chunk 状态保存了什么

每个 `csid` 都对应一个 `ChunkStreamState`，里面保存：

- `timestamp`
- `timestampDelta`
- `extendedTimestamp`
- `messageLength`
- `typeId`
- `messageStreamId`
- `bytesRead`
- `payload`
- `headerInitialized`

见 [RtmpChunkParser.h](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.h:25)

你可以把它理解成：

- 这是“该 csid 最近一次 message header + 当前正在组装的 message 进度”

## 11. 当前项目已经支持到什么程度

现在这版代码已经支持：

- 握手完成后进入 chunk 层
- 三种长度的 basic header
- 四种 `fmt`
- extended timestamp
- 半包
- 大 message 按 `inChunkSize` 分片重组
- 收到 `Set Chunk Size` 后更新入站 chunk size

相关接入点在：

- [RtmpServer.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpServer.cc:97)

## 12. 当前项目还没做什么

这份代码目前还没有进入：

- AMF0 解码
- `connect`
- `createStream`
- `publish`
- `play`
- metadata/audio/video 分发

也就是说，现在解析器已经能产出完整的 `RtmpMessage`，但还没有解释 payload 里的业务语义。

## 13. 如何理解 type id

`type id` 表示这个 message 的类型。

当前项目里已经定义了一些常见类型，见：

- [RtmpTypes.h](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpTypes.h:15)

例如：

- `1`：Set Chunk Size
- `5`：Window Acknowledgement Size
- `6`：Set Peer Bandwidth
- `8`：Audio
- `9`：Video
- `18`：Data Message AMF0
- `20`：Command Message AMF0

当前 `RtmpServer` 已经先特别处理了：

- `Set Chunk Size`

对应代码：

- [RtmpServer.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpServer.cc:112)

## 14. 学习建议

如果你现在正在跟代码，建议这样读：

1. 先读 basic header  
   从 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:112) 开始

2. 再读 `fmt=0/1/2/3`  
   看 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:152)

3. 再看 payload 是怎么拼装的  
   看 [RtmpChunkParser.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpChunkParser.cc:85)

4. 最后再回到 server 接入点  
   看 [RtmpServer.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpServer.cc:97)

## 15. 一句话总结

`fmt` 决定“这次 chunk header 相比上一次同一 csid 的 header 省略了多少字段”；  
`csid` 决定“我该去复用哪条 chunk stream 的历史状态”；  
解析器的工作就是把这些分片状态重新还原成完整的 `RtmpMessage`。
