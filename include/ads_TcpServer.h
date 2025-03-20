#pragma once

// TcpServer 类是 muduo 网络库中的一个核心组件，封装了 基于 Reactor 模型的 TCP 服务器

#include <functional>      // 用于存储回调函数
#include <string>
#include <memory>          // std::unique_ptr  std::shared_ptr 智能指针，避免手动管理对象生命周期。 
#include <atomic>          // 保证 多线程 访问 started_ 变量时的 原子性，避免竞态条件。
#include <unordered_map>   // 哈希表，用于存储所有的 TCP 连接。

#include "ads_EventLoop.h"
#include "ads_Acceptor.h"
#include "ads_InetAddress.h"
#include "ads_noncopyable.h"
#include "ads_EventLoopThreadPool.h"
#include "ads_Callbacks.h"
#include "ads_TcpConnection.h"
#include "ads_Buffer.h"

class TcpServer{
public:
    using ThreadInitCallback = std::function<void(EvenLoop *)>;

    // 端口复用选项，决定是否允许多个套接字绑定同一端口。
    enum Option{
        kNoReusePort;
        kReusePort;
    };

    TcpServer(EventLoop *loop,
              const InetAddress *listenAddr,
              const std::string &nameArg,
              Option option = kNoReusePort);
    ~TcpServer();

    // 用于在 线程池 中，每个线程的 EventLoop 初始化时调用。
    void setThreadInitCallback(const ThreadInitCallback &cb) {threadInitCallback_ = cb;}
    // 有 新连接 时调用（accept 一个连接）
    void setConnectionCallback(const ConnectionCallback &cb) {connectionCallback_ = cb;}
    // 收到 消息 时调用
    void setMessageCallback(const MessageCallback &cb) {messageCallback_ = cb;}
    // 发送 完成 时调用
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) {writeCompleteCallback_ = cb;}

    // 设置 工作线程数量，底层采用 one loop per thread 模型，每个线程拥有一个 EventLoop。
    void setThreadNum(int numThreads)

    // 启动服务器，如果没有监听，就开始监听新连接。
    // 这个函数是 线程安全 的，可以被多个线程调用，但仅第一次调用会生效。
    void start();


private:
    // 处理新的Tcp连接
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    // 在 EventLoop 线程 中安全地移除连接。
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    // 存储所有的 TCP 连接，key 是连接 ID，value 是 TcpConnectionPtr（std::shared_ptr<TcpConnection>）。
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    // 服务器主循环baseloop_（通常在 主线程 运行）。
    EventLoop *loop_;

    const std::string ipPort_;  // 存储ip和端口信息
    const std::string name_;    // 存储服务器名称

    // acceptor_ 是 监听套接字，负责接受新连接。Acceptor 运行在 main Reactor，接受连接后，交给 sub Reactor 处理。
    std::unique_ptr<Acceptor> acceptor_;
    // threadPool_ 负责 管理工作线程池，实现 one loop per thread。
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ThreadInitCallback threadInitCallback_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallbck writeCompleteCallback_;

    int numThreads_;     // 线程池中线程数量
    std::atomic_int started_;    // 是否已启动，保证线程安全
    int nextConnId_;     // 下一个连接的ID，用于生成唯一连接名称
    ConnetionMap connetions_;    // 存储 所有的 TCP 连接，可以快速查找和管理。
    
};

