#include <functional>
#include <string.h>

#include <ads_TcpServer.h>
#include <ads_TcpConnection.h>
#include <ads_Logger.h>


static EventLoop *CheckLoopNotNull(EventLoop *loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option)
    // loop_ 是 TcpServer 依赖的 主 Reactor，CheckLoopNotNull 确保它合法。
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kNoReusePort))
    , threadPool_(new EventLoopThreadPool(loop, nameArg))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    // Acceptor 监听到新连接 后会调用 newConnection(sockfd, peerAddr)。
    // 这里使用 std::bind 绑定 TcpServer::newConnection，并传递 sockfd 和 peerAddr。
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for(auto &item : connections_){
        // item 代表 每个键值对，item.second代表TcpConnectionPtr（连接对象的 shared_ptr）
        TcpConnectionPtr conn(item.second);
        // reset() 会让 connections_ 里的 shared_ptr 失效（引用计数减 1）。原始智能指针复位
        // 但是 conn 仍然持有该 shared_ptr，确保 TcpConnection 不会立即销毁。
        // connections_ 里不再保存该连接（从哈希表中移除 shared_ptr）。
        // 仍然让 conn 在 getLoop()->runInLoop() 中 延迟销毁，保证正确的线程上下文。
        item.second.reset();
        // getLoop() 获取 conn 所属的 EventLoop（sub Reactor）。
        // runInLoop() 确保 connectDestroyed() 在 正确的线程 执行（避免跨线程调用）。
        // connectDestroyed() 负责 关闭连接、释放资源。
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
        /**
         * 首先TcpServer::connections_是一个unordered_map<string, TcpConnectionPtr>，其中TcpConnectionPtr的含义是指向TcpConnection的shared_ptr。
            在一开始，每一个TcpConnection对象都被一个共享智能指针TcpConnetionPtr持有，当执行了TcpConnectionPtr conn(item.second)时，这个TcpConnetion对象就被conn和这个item.second共同持有，但是这个conn的生存周期很短，只要离开了当前的这一次for循环，conn就会被释放。
            紧接着调用item.second.reset()释放掉TcpServer中保存的该TcpConnectino对象的智能指针。此时在当前情况下，只剩下conn还持有这个TcpConnection对象，因此当前TcpConnection对象还不会被析构。
            接着调用了conn->getLoop()->runInLoop(bind(&TcpConnection::connectDestroyed, conn));这句话的含义是让Sub EventLoop线程去执行TcpConnection::connectDestroyed()函数。当你把这个conn的成员函数传进去的时候，conn所指向的资源的引用计数会加1。因为传给runInLoop的不只有函数，还有这个函数所属的对象conn。
            Sub EventLoop线程开始运行TcpConnection::connectDestroyed()函数。
            Main EventLoop线程当前这一轮for循环跑完，共享智能指针conn离开代码块，因此被析构，但是TcpConnection对象还不会被释放，因为还有一个共享智能指针指向这个TcpConnection对象，而且这个智能指针在TcpConnection::connectDestroyed()中，只不过这个智能指针你看不到，它在这个函数中是一个隐式的this的存在。当这个函数执行完后，智能指针就真的被释放了。到此，就没有任何智能指针指向这个TcpConnection对象了。TcpConnection对象就彻底被析构删除了。
        */
    }
}

// 设置 EventLoopThreadPool 的线程数量，即 subloop 的个数（Worker 线程数）
void TcpServer::setThreadNum(int numThreads){
    numThreads_ = numThreads;
    threadPool_->setThreadNum(numThreads_);
}

void TcpServer::start(){
    // fetch_add(1) 原子操作：
    // 读取 started_ 的当前值。加 1（fetch_add 返回加之前的值）。
    // 只有 started_ == 0 时才会执行 start() 的逻辑，否则直接返回。
    // 目的：防止 start() 被多次调用，避免重复启动服务器。
    if(started_.fetch_add(1) == 0){
        // threadInitCallback_ 是 用户自定义的回调，可以用来初始化 subloop。
        threadPool_->start(threadInitCallback_);
        // acceptor_ 是 std::unique_ptr<Acceptor>，需要 get() 获取原始指针。
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 当Acceptor监听到新连接是，acceptor_执行这个会掉，将新连接sockfd分配给subLoop，创建TcpConnection，并设置相应回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr){
    // 轮询选择一个subLoop来管理新的连接
    EventLoop *ioLoop = threadPool_->getNextLoop();

    // 生成新连接的名称
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    // 如ServerName-127.0.0.1:8080#2
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
              name_.c_str(), connName.c_str(), ipPort_.c_str());

    // 获取sockfd绑定的本地IP和端口
    // sockaddr_in 是 IPv4 套接字地址结构
    sockaddr_in local; 
    // 清空 local 结构体，确保所有字节初始化为 0，避免出现脏数据
    ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);   // socklen_t 是 sockaddr 结构体长度 的类型
    // getsockname()获取sockfd绑定的本地地址
    // sockfd：要查询的 套接字文件描述符
    // addr：存储本地地址的 输出参数（传入 &local）
    // addrlen：输入/输出参数，表示 addr 的大小
    if(::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0){
        LOG_ERROR("sockets::getLocalAddr");
    }
    // 封装一下local
    InetAddress localAddr(local);

    // 创建TcpConnection
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;  //放入哈希表

    // 设置回调，用户设置给TcpServer -> TcpConnectcion。至于Channel则是绑定TcpConnection 里的handlexxx，而handlexxx又包含了下面设置的回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // removeConnection 是 TcpServer 的成员函数，因此需要一个 TcpServer 实例才能调用。
    // this 代表当前 TcpServer 对象，使 removeConnection 绑定到该对象。
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn){
    // 将 removeConnectionInLoop(conn) 投递到 loop_ 线程执行，防止多线程并发访问 connections_
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn){
    LOG_INFO("TcpServer::removeConnecitonInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());
    // 在当前server的loop线程中访问connections_，移除该conn，避免多线程并发访问connections_
    connections_.erase(conn->name());
    // 获取conn所属subLoop
    EventLoop *ioLoop = conn->getLoop();
    // queueInLoop() 保证 connectDestroyed() 在 conn 所属的 subLoop 中安全执行，避免多线程问题。
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
