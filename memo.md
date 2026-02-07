### 🔑 rmuduo 资源所有权与借用关系表

| 资源 | 真正拥有者 (Owner) | 借用者 (User) | 职责描述 |
| :--- | :--- | :--- | :--- |
| **listenfd** | `Acceptor` | `EventLoop` / `Poller` | `Acceptor` 负责生命周期，`Poller` 负责监听 |
| **connfd** | `TcpConnection` | `EventLoop` / `Poller` | `TcpConnection` 析构时自动调用 `close(fd)` |
| **Channel** | `Acceptor` 或 `TcpConnection` | `EventLoop` / `Poller` | 作为 Fd 的包装，分发事件回调 |
| **EventLoop** | `EventLoopThread` 栈 | `Acceptor` / `TcpConnection` | 提供运行时的 Reactor 循环环境 |
| **TcpConnection** | `TcpServer` | | |

baseloop 来自于用户使用时创建，subloop由threadpool管理，创建在每个线程的栈上，生命周期与线程同步。

将const shared_ptr& 作为参数去接受一个右值如shared_from_this，这时仅在构造shared_from_this时发生一次计数增加，函数结束后技术减少，const&将这个右值的生命周期延长至了函数结束。
将const shared_ptr 作为参数去接受一个右值如shared_from_this，将会在构造shared_from_this时发生一次计数增加，传给参数时发生一次计数增加，函数结束减1，shared_from_this所在表达式结束减1；
将const shared_ptr& 作为参数去接受一个左值，当左值失效后会导致悬空引用。