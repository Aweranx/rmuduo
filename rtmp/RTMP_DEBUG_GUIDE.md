# RTMP 联调排查清单

这份文档的目标不是讲协议，而是帮你在本项目里快速定位：

- 为什么客户端连不上
- 为什么能 `connect` 但不能 `publish`
- 为什么能推流但不能 `play`
- 为什么新 player 连上后没画面

当前示例程序入口：

- [example/rtmp_server_test.cc](/home/ranx/work/viewcode/rmuduo/example/rtmp_server_test.cc:1)

## 1. 启动方式

先构建：

```bash
cmake -S . -B build
cmake --build build -j2
```

再启动 RTMP 服务：

```bash
./build/bin/rtmp_server_test
```

默认：

- 端口：`1935`
- 线程数：`2`

也可以手动指定：

```bash
./build/bin/rtmp_server_test 1935 2
```

## 2. 最小联调顺序

推荐严格按这个顺序测，不要一上来就同时开 OBS 和播放器。

### 2.1 先验证推流

```bash
ffmpeg -re -stream_loop -1 -i input.mp4 -c copy -f flv rtmp://127.0.0.1:1935/live/test
```

这一步只看服务端日志是否出现：

- `accepted connection`
- `handshake completed`
- `parsed AMF0 command ... name=connect`
- `handling createStream`
- `publish start`

如果这些都出现，说明控制面已经通了。

### 2.2 再验证播放

另开一个终端：

```bash
ffplay rtmp://127.0.0.1:1935/live/test
```

这一步只看服务端日志是否出现：

- `accepted connection`
- `handshake completed`
- `parsed AMF0 command ... name=connect`
- `handling createStream`
- `play start`

如果随后还能看到：

- `broadcast media`

说明 publisher 的媒体消息已经进入 session，并开始向 player 转发。

### 2.3 最后再用 OBS

OBS 参数：

- `Server`: `rtmp://127.0.0.1:1935/live`
- `Stream Key`: `test`

先只开推流，不要同时开录制和复杂转码选项。

## 3. 日志和阶段对应关系

当前实现里，定位问题最有效的方法就是把日志和协议阶段对上。

### 阶段 A：TCP 接入

日志：

- [RtmpServer.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpServer.cc:53) `accepted connection`
- [RtmpServer.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpServer.cc:60) `closed connection`

如果连 `accepted connection` 都没有：

- 服务没启动
- 端口不对
- 客户端连错地址
- 防火墙/监听地址有问题

### 阶段 B：RTMP 握手

日志：

- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:52) `handshake failed`
- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:67) `handshake completed`

如果有连接，但没有 `handshake completed`：

- 客户端不是按 RTMP 连
- 握手字节没收全
- 连接很快被对端断开

### 阶段 C：Chunk / Message 解析

日志：

- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:77) `chunk parse failed`
- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:89) `parsed message`

如果握手成功后马上报 `chunk parse failed`：

- chunk header 解析有误
- 收到当前实现还没支持的报文形态
- `fmt/csid/timestamp` 复用状态不一致

### 阶段 D：AMF0 Command

日志：

- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:119) `parsed AMF0 command`

如果有 message 但没有 command：

- 说明当前拿到的是媒体消息或控制消息，不是 `Command Message AMF0`

如果 command 解不出来：

- 重点看 `Amf0Decoder`
- 检查 command payload 是否真的走 AMF0

### 阶段 E：控制面

关键日志：

- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:193) `handling connect`
- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:210) `handling createStream`
- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:267) `publish start`
- [RtmpConnection.cc](/home/ranx/work/viewcode/rmuduo/rtmp/RtmpConnection.cc:314) `play start`

这几条可以直接回答“卡在哪一步”。

## 4. 常见现象怎么判断

### 现象 1：FFmpeg 一连就断

先看服务端有没有：

- `accepted connection`
- `handshake completed`

判断：

- 没有 `accepted connection`：网络或端口问题
- 有 `accepted connection` 没有 `handshake completed`：卡在握手
- 有 `handshake completed` 但没有 `parsed AMF0 command name=connect`：卡在 chunk/message 解析

### 现象 2：有 `connect`，但没有 `publish start`

重点看是否有：

- `handling createStream`
- `publish arrived before createStream`
- `publish stream id mismatch`
- `publish missing stream name`

这类问题通常不是 TCP 问题，而是 command 语义层的问题。

### 现象 3：publisher 正常，但 player 一直黑屏

重点看：

- 有没有 `play start`
- 有没有持续的 `broadcast media`

判断：

- 没有 `play start`：player 控制面没走通
- 有 `play start` 但没有 `broadcast media`：publisher 没有进媒体面，或者 publisher 已掉线
- 有 `broadcast media` 但仍黑屏：通常是缓存、时间戳、编码头或播放器兼容性问题

### 现象 4：新 player 进来很久才出画

当前实现已经补了：

- metadata 缓存
- AAC sequence header 缓存
- AVC sequence header 缓存
- 最小 GOP 缓存

如果仍然很慢，重点怀疑：

- GOP 太长，关键帧间隔过大
- 推流端没有按预期发送 sequence header
- 你推的是当前实现没细处理的编码格式

## 5. 推荐联调命令

### 用 FFmpeg 推流

```bash
ffmpeg -re -stream_loop -1 -i input.mp4 -c copy -f flv rtmp://127.0.0.1:1935/live/test
```

如果 `-c copy` 不稳定，可以改成显式编码：

```bash
ffmpeg -re -stream_loop -1 -i input.mp4 \
  -c:v libx264 -preset veryfast -g 50 \
  -c:a aac \
  -f flv rtmp://127.0.0.1:1935/live/test
```

### 用 FFplay 拉流

```bash
ffplay rtmp://127.0.0.1:1935/live/test
```

### 只看 FFmpeg 的协议日志

```bash
ffmpeg -loglevel verbose -re -stream_loop -1 -i input.mp4 -c copy -f flv rtmp://127.0.0.1:1935/live/test
```

如果你需要更细的客户端侧日志，这个很有用。

## 6. 现在这版实现的边界

当前代码适合拿来联调学习，但还不是完整 RTMP 服务端。

已经有的：

- 握手
- chunk/message 解析
- `connect`
- `createStream`
- `publish`
- `play`
- `deleteStream`
- `closeStream`
- metadata / sequence header / 最小 GOP 缓存

还比较薄弱的：

- metadata 没做结构化解析
- 没做更完整的控制消息处理
- 没做更严格的时间戳和回放策略
- 没做 codec 兼容性扩展
- 没做统计和监控

## 7. 联调时最实用的策略

每次只验证一层，不要混着猜。

顺序固定成：

1. 先看 TCP 连接有没有进来
2. 再看握手有没有完成
3. 再看 `connect/createStream/publish/play`
4. 最后才看媒体转发和首帧问题

这样排障速度会快很多。
