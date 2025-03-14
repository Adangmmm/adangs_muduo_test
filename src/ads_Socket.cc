#include <unistd.h>
#include <sys/sypes.h>
#include <sys/socket.h>
#include <netinet/tcp/h>
#include <string.h>

#include "ads_Socket.h"
// #include "ads_Logger.h"
#include "InetAddress.h"

Socket::~Socket(){
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr){
    /* ::bind() 是系统调用，定义在 <sys/socket.h> 头文件中
     * sockaddr* 是一个通用的 套接字地址 结构体指针，用于表示各种类型的套接字地址。
     * bind成功返回 0 ，失败返回 -1 ，并设置 errno 错误码。
     */
    if(0 != ::bind(sockfd_, (sockaddr *)localaddr.getSockAddr()), sizeof(sockaddr_in)){
        LOG_FATAL("bind sockfa:%d fail\n", sockfd_);
    }
}

// 将 socket 从普通的 socket 状态转换为监听状态，用于服务器场景。
void Sockt::listen(){
    /* ::listen() 是一个系统调用，定义在 <sys/socket.h> 头文件中
     * 1024 → backlog 参数，表示等待连接队列的最大长度。
     * backlog 定义了操作系统内核为这个 socket 维护的未完成连接队列的长度。
     * 连接分为以下两种状态：
     *      半连接（SYN_RCVD）：TCP 三次握手的 SYN 阶段，尚未完成握手。
     *      全连接（ESTABLISHED）：三次握手成功，等待 accept() 处理。
     * listen成功返回 0 ，失败返回 -1 ，并设置 errno 错误码。
     */
    if(0 != ::listen(sockfd_, 1024)){
        LOG_FATAL("listen sockfd:%d faiil\n), sockfd_");
    }
}

// 返回新创建的已连接 socket 文件描述符（connfd）。失败，返回 -1。
int Socket::accpept(InetAddress *peeraddr){
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    // SOCK_NONBLOCK → 将新 socket 设置为非阻塞。SOCK_CLOEXEC → 设置 close-on-exec 标志，避免子进程继承该 socket。
    // 如果不设置 SOCK_CLOEXEC，子进程会继承已连接的 socket 文件描述符：即使父进程关闭了 socket，子进程仍持有描述符，可能导致端口被占用或资源泄漏。服务器需要在 fork() 后自行关闭不必要的描述符。
    int connfd = ::ccept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if(connfd >= 0){
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::serTcpNoDelay(bool on){
    // TCP_NODELAY -> 禁用Nagle算法
    // Nagle算法用于减少网络上传输的小数据包量
    // TCP_NODELAY设置为1后，禁用Nagle算法，允许小数据包的立即发送
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    // 启用 Nagle 算法： 适合需要优化带宽使用的场景
    // 禁用 Nagle 算法： 适合低延迟、高频小包传输的场景
}

void Socket::setReuseAddr(bool on){
    // SO_REUSEADDR -> 允许多个 socket 绑定到同一个 IP 地址和端口号
    // 服务器重启后快速重新绑定到同一个端口
    // 多个 socket 在不同的进程中同时监听相同的地址和端口
    // 避免socket因进入 TIME_WAIT 状态导致端口无法及时释放
    int optval = on ? 1 : 0;
    ::setsockopt(sockfa_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}
// SO_REUSEADDR → 解决 TIME_WAIT
// SO_REUSEPORT → 解决负载均衡
void Socket::setReusePort(bool on){
    // SO_REUSEPORT -> 允许多个 socket 绑定到同一个端口
    // 与 SO_REUSEADDR 不同：
    //      SO_REUSEADDR 允许不同进程（或不同主机）绑定相同的 IP 地址和端口，但只允许其中一个 socket 处理连接。
    //      SO_REUSEPORT 允许多个 socket 绑定相同端口，并且可以同时接收连接，从而在不同线程或进程之间实现负载均衡。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on){
    // SO_KEEPALIVE -> 启用在已建立的TCP连接上，定期发送探测包（Keep_Alive）包
    // 若对端在规定时间没响应，则认为连接已断开
    // 若响应，则保持连接存活状态
    // 如果连接已断开，内核会触发 ECONNRESET 或 ETIMEDOUT 错误，recv() 或 send() 将返回 -1。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval);)
}