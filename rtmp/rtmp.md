## 大致架构
RtmpServer  管理多个session
RtmpConnection 作为TcpConnection的context出现，真正处理上层rtmp
RtmpSession  管理多个conn, 1 v N,一个推流，多个拉流
AMF
RtmpHandShake
RtmpChunk

## 流程
名称,大小,内部结构,核心作用
C0,1B,版本号（通常为 0x03）,客户端发起协议版本请求
S0,1B,版本号（通常为 0x03）,服务端确认协议版本支持
C1,1536B,时间戳(4B) + 零值(4B) + 随机数(1528B),客户端提供同步基准和随机序列
S1,1536B,时间戳(4B) + 零值(4B) + 随机数(1528B),服务端提供同步基准和随机序列
C2,1536B,对方 S1 的完整镜像拷贝,客户端确认已收到服务端的 S1
S2,1536B,对方 C1 的完整镜像拷贝,服务端确认已收到客户端的 C1

client向server发出c0c1，server传回s0s1s2，client发出c2，握手结束。