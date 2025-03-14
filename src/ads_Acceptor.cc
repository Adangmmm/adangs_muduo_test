#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "ads_Acceptor.h"
#include "ads_Logger.h"
#include "ads_InetAddress.h"

// 静态辅助函数 创建非阻塞的socket
/* 创建socket：int socket(int domain, int type, int protocol);
 * AF_INET：使用 IPv4 地址族。
 * SOCK_STREAM：使用 TCP 协议（面向连接，可靠传输）。
 * SOCK_NONBLOCK：将 socket 设置为非阻塞模式。
 * 非阻塞模式下，调用 accept()、read() 等操作如果没有数据，立即返回 EAGAIN 错误，而不是阻塞等待。
 * SOCK_CLOEXEC：在 fork() 执行 exec 时自动关闭 socket（防止文件描述符泄漏）。
 * IPPROTO_TCP：使用 TCP 协议。
 */
static int createNonblocking(){
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_ NONBLOC | SOCK_CLOEXEC, IPPROTO_TCP);
    if(sockfd < 0){
        /* LOG_FATAL 是一个宏或日志函数，输出致命级别的错误消息，并可能会终止程序。
         * __FILE__、__FUNCTION__、__LINE__ 分别表示：
         * __FILE__：当前源文件名
         * __FUNCTION__：当前函数名
         * __LINE__：当前行号
         * errno：系统全局变量，记录最近一次系统调用的错误号。
         */
        LOG_FATAL("%s:%s:%d listen socket create error:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    // 设置socket选项，SO_REUSEADDR表示允许重用 TIME_WAIT 状态的端口。常见于服务重启时，避免"地址已被占用"的问题。
    acceptSocket.setReuseAddr(true);
    // 设置socket选项，SO_REUSEPORT表示允许多个 socket (进程/线程)绑定到同一个端口，用于负载均衡。
    acceptSocket.setReusePort(true);
    // 将 socket 绑定到指定的 IP 地址和端口。调用 bind() 系统调用。如果绑定失败，可能是由于地址被占用或权限不足。
    acceptSocket.bindAddress(listenAddr);
    // TcpServer::start() => Acceptor.listen() 如果有新用户连接 要执行一个回调(accept()生成新的文件描述符 => connfd => 打包成Channel => 唤醒subloop)
    // baseloop监听到有事件发生 => acceptChannel_(listenfd) => 执行该回调函数
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor(){
    acceptChannel_.disableAll();   // Poller不再对该Channel（fd）的任何事件感兴趣
    acceptChannel_.remove();       // 从Poller中移除该Channel
    /* disableAll + remove 的目的：
     * 1. 避免空指针访问
     * 如果不先取消事件监听，可能在对象析构后仍然有事件触发，导致访问已销毁的对象。
     * 2. 防止内存泄漏
     * 如果没有删除 Channel，可能导致 Poller 持有无效指针，造成内存泄漏。
     * 3. 防止事件“悬挂”
     * 如果不及时删除 socket，可能在 epoll_wait() 中出现无效事件，导致程序异常或 busy loop（空轮询）。
    */
}

Acceptor::listen(){
    listenning_ = true;              // 标志位
    acceptSocket_.listen();          // 这一行是底层网络通信的核心部分，调用的是封装在 Socket 类中的 listen() 方法
    acceptChannel_.enableReading();  // 将 acceptChannel_ 注册到 Poller 中，监听其读事件，关键！
    /* 触发机制：
     * 1.listen() 后，socket 开始监听新连接。
     * 2.新连接到来时，内核触发 EPOLLIN 事件。
     * 3.Poller 监听到 EPOLLIN 事件，通知 EventLoop。
     * 4.EventLoop 调用 Channel 的回调（即 Acceptor::handleRead()）。
     * 5.Acceptor::handleRead() 调用 accept() 接受新连接。
     * 6.将新连接封装成 TcpConnection，交由子线程（subloop）管理。
     */
}

Acceptor::handleRead(){
    // InetAddress 是 muduo 封装的一个类，封装了IP 地址和端口号。
    // accept() 系统调用会将客户端的 IP 和端口号写入 peerAddr。peerAddr 用于保存新连接客户端的地址信息。
    InetAddress peerAddr; 
    // 调用封装在 acceptSocket_（类型为 Socket）中的 .accept() 方法。.accept() 方法内部调用 accept() 系统调用，接受新的客户端连接。
    // 如果连接成功，返回新的 socket 文件描述符（connfd）。如果失败，返回 -1，并设置 errno 表示错误原因。
    int connfd = acceptSoket_.accept(&peerAddr);

    if(connfd >= 0){
        if(NewConnectionCallback_){
            // NewConnectionCallback_ 是用户在 Acceptor 中设置的回调函数。在 TcpServer 的构造函数中设置。
            NewConnectionCallback_(connfd, peerAddr);
        }
        else{
            // 如果未设置回调函数，说明上层未定义如何处理新连接。为避免文件描述符泄漏，直接关闭新连接。
            ::close(connfd);
        }
    }
    else{
        // 如果 accept() 返回 -1，输出错误日志。errno 表示具体的错误类型。
        LOG_ERROR("%s:%s:%d accept error:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        // EMFILE 表示当前进程打开的文件描述符数量已达到上限。
        if(errno == EMFILE){
            LOG_ERROR("%s:%s:%d sockfd reached limit\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }    
}