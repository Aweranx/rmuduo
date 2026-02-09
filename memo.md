### 🔑 rmuduo 资源所有权与借用关系表

| 资源 | 真正拥有者 (Owner) | 借用者 (User) | 职责描述 |
| :--- | :--- | :--- | :--- |
| **listenfd** | `Acceptor` | `EventLoop` / `Poller` | `Acceptor` 负责生命周期，`Poller` 负责监听 |
| **connfd** | `TcpConnection` | `EventLoop` / `Poller` | `TcpConnection` 析构时自动调用 `close(fd)` |
| **Channel** | `Acceptor` 或 `TcpConnection` | `EventLoop` / `Poller` | 作为 Fd 的包装，分发事件回调 |
| **EventLoop** | `EventLoopThread` 栈 | `Acceptor` / `TcpConnection` | 提供运行时的 Reactor 循环环境 |
| **TcpConnection** | `TcpServer` | | |


# 怎么才算runinloop

## 大体架构
TcpServer里有Accptor、conns、mainloop和sublooppool，共 1+N(sublooppool)个线程。
Accptor的listen绑定在mainloop上执行，conns各自在内部存储自己所绑定的loop。
他们两个都有有自己的fd和设置了感兴趣事件以及相应回调的channel。
当有新连接时，Accptor调用TcpServer给出的回调，选出一个subloop然后用fd和addr构造出一个conn。

### wakeup
各loop在创建的时候初始化一个eventfd和channel，该fd被注册在loop的epoll中。
作用是打破epoll_wait的阻塞，让他向下处理loop的回调队列中的任务。
不然当前epoll中没有其他消息源，将会导致回调队列任务一直不执行。


### 1. **回调执行链路** 
```
当需要添加或改变已有channel时，需要使用runInLoop将要进行的行为加入队列并wakeup其所属loop在poll执行完后执行。
已有的channel的回调是等待poll返回后自动执行的。
Poller.poll() → activeChannels_(就绪) → Channel::handleEvent() 
→ readCallback/writeCallback/closeCallback/errorCallback 
→ TcpConnection的handler(handleRead/handleWrite/handleClose/handleError)
因为有可能会把这些操作交给业务线程池处理，所以需要判断一下isInLoopThread()，如果在loop所在线程就直接执行，
不在的话就把函数对象扔进loop的队列，防止造成多线程竞争
```

### 2. **发送缓冲区管理策略**  
- 如果buffer为空且当前无EPOLLOUT，直接write，若一次发不完则剩余存入buffer并注册EPOLLOUT
- 如果buffer有数据，新数据追加到buffer，channel保持EPOLLOUT以持续发送
- 回调链：Poller发现EPOLLOUT → handleWrite() → 继续发送buffer中数据 → buffer清空后取消EPOLLOUT并触发writeCompleteCallback
- 利用iovec结合栈内存实现读取未知大小数据

### 3. **对象生命周期保护** 
- **Channel::tie()** 使用weak_ptr保存TcpConnection，在handleEvent时lock()检查，防止IO事件处理中对象被提前销毁
- baseloop 来自于用户使用时创建，subloop由threadpool管理，创建在每个线程的栈上，生命周期与线程同步。
- 将const shared_ptr& 作为参数去接受一个右值如shared_from_this，这时仅在构造shared_from_this时发生一次计数增加，函数结束后技术减少，const&将这个右值的生命周期延长至了函数结束。
- 将const shared_ptr 作为参数去接受一个右值如shared_from_this，将会在构造shared_from_this时发生一次计数增加，传给参数时发生一次计数增加，函数结束减1，shared_from_this所在表达式结束减1；
- 将const shared_ptr& 作为参数去接受一个左值，当左值失效后会导致悬空引用。

### 4. **连接关闭的状态转换与触发** 
- 主动关闭：`shutdown()` → 设置state为kDisconnecting → sendInLoop检查buffer发完后调shutdownInLoop → `socket_->shutdownWrite()` 关闭写端
- 被动关闭：Poller检测到EPOLLHUP → handleClose() → state设为kDisconnected → 触发connectionCallback和closeCallback → TcpServer::removeConnection清理
- conn调用shutdownwrite后，接收client返回后，channel调用closecallback(TcpServer::removeconn)，删除channel和conn

### 5. **跨线程任务同步机制** 
- **eventfd** 作为wakeup工具，跨线程通知loop有新任务需要处理
- **queueInLoop** 加入pendingFunctors_，在poll后由doPendingFunctors执行
- **callingPendingFunctors_标志**：防止"执行回调完毕→阻塞poll→新回调来临→无法及时处理"的死锁，新回调到来时强制wakeup loop

### 6. **高水位标记回调** 
- 当 `outputBuffer.readableBytes() + newData >= highWaterMark_` 时触发
- 应用层可在此降速或采取流控措施，防止内存爆炸

### 7.定时器
- 注册timerfd在loop中，当有timer到期时通知channel自己取出所有的到时timer执行回调。


