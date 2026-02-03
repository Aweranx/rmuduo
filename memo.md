# rmuduo 网络库架构设计文档

本项目参考 `muduo` 实现，基于 **Reactor** 模式构建高性能多线程网络服务器框架。

## 1. 核心类图 (Class Diagram)

描述了库中主要组件的所有权（Composition）与关联（Association）关系。

```mermaid
classDiagram
    class EventLoop {
        -scoped_ptr~Poller~ poller_
        -vector~Channel*~ activeChannels_
        +loop()
        +runInLoop(Functor)
    }

    class Poller {
        <<interface>>
        #map~int, Channel*~ channels_
        +poll(timeout, ChannelList*)
        +updateChannel(Channel*)
    }

    class Channel {
        -int fd_
        -int events_
        -ReadCallback readCallback_
        +handleEvent()
        +setReadCallback(cb)
    }

    class TcpConnection {
        -scoped_ptr~Socket~ socket_
        -scoped_ptr~Channel~ channel_
        -Buffer inputBuffer_
        -Buffer outputBuffer_
        +send(data)
        +shutdown()
    }

    class Acceptor {
        -Socket acceptSocket_
        -Channel acceptChannel_
        -NewConnectionCallback cb_
        +listen()
    }

    EventLoop "1" *-- "1" Poller : 拥有并驱动
    EventLoop "1" o-- "n" Channel : 管理其生命周期中的事件分发
    TcpConnection "1" *-- "1" Channel : 包含用于处理业务IO
    Acceptor "1" *-- "1" Channel : 包含用于处理新连接监听
    TcpConnection ..> EventLoop : 在指定的Loop中运行
    Acceptor ..> EventLoop : 在指定的Loop中运行