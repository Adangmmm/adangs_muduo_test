#pragma once

#include "noncopyable.h"

class InetAddress;

class Socket : noncopyable{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    { 
    }
    ~Socket();

    int fd() const {return sockfd_;}
    void bindAddress(const InetAddress &localaddr);
    void listen();  // 将socket转换为监听状态，供accept()调用
    int accept(InetAddress *peeraddr);  // 接受新连接，返回一个新的socket文件描述符，用于与客户端通信

    // 关闭socket的写入方向，表示不再发送数据，但仍然可以接收数据
    void shutdownWrite();

    // 设置 TCP_NODELAY 选项，禁用 Nagle 算法，减少延迟。
    void setTcpNoDelay(bool on);
    // 设置 SO_REUSEADDR 选项，允许重用本地地址。（重用 TIME_WAIT 状态的端口）
    void ReuseAddr(bool on);
    // 设置 SO_REUSEPORT 选项，允许多个socket绑定到相同端口
    void setReusePort(bool on);
    // 设置 SO_KEEPALIVE 选项，启用 TCP 的 KeepAlive 机制，探测空闲连接是否仍然存活。
    void setKeepAlive(bool on);

    private:
    const int sockfd_;
};