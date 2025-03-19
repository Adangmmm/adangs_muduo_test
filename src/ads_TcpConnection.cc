#include <functional>
#include <string>
#include <errno.h>
#include <sys/types.h>
// 提供socket() send() recv() shutdown() 等函数
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
// 用于sendfile()零拷贝文件传输
#include <sys/sendfile.h>
//文件操作，如open()
#include <fcntl.h>
// 系统调用，如close()关闭fd
#include <unistd.h>

#include "ads_TcpConnection.h"
#include "ads_Logger.h"
#include "ads_Socket.h"
#include "ads_Channel.h"
#include "ads_EvenLoop.h"


static EventLoop *CheckLoopNotNull(EventLoop *loop){
    if(loop_ == nullptr){
        LOG_FATAL9("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION, __LINE__);
    }
    else{
        return loop;
    }
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    // 高水位阈值，即数据缓冲区达到 64MB 时触发高水位回调。
    , highWaterMark_(64 * 1024 *1024)
{
    // 绑定Channel回调
    // std::placeholders::_1：占位符，对应 handleRead(Timestamp recvTime) 的参数。
    channel_->setReadCallback(
        std::bind(&TcpConnection::hadleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConneciton::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    // const char* std::string::c_str() const noexcept; 
    // name_.c_str()作用是 将 std::string 转换为 C 风格字符串（const char*）。
    // .c_str()不会创建新数据，而是直接指向 std::string 的内部存储。
    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    // 让 内核定期发送 keep-alive 探测包，检测 连接是否存活。如果对端异常断开（如断网），避免 死连接 占用资源。
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d\n", name.c_str(), sockfd);
}


// 当客户端发送数据时，服务器检测到EPOLLIN事件，调用handleRead()读取数据
void TcpConnection::handleRead(Timestamp receiveTime){
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    //有数据到达
    if(n > 0){
        // 通知上层应用有数据可读，调用用户注册的onMessage回调
        // sahre_from_this()确保TcpCOnnection在处理回调期间不会被销毁
        // 用户可以在onMessage()里面解析inputBuffer，执行其业务逻辑
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    // 读取0说明客户端断开了
    else if(n == 0){
        handleClose();
    }
    // n < 0说明出错了
    else{
        // 常见的错误：ECONNRESET：对端异常关闭 EAGAIN：数据未准备好 EINTR：被信号中断
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite(){
    // 检查Channel是否仍在监听EPOLLOUT事件
    if(channel_->isWriting()){
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);

        if(n > 0){
            // 移动readerIndex_，表示已经读取n字节
            outputBuffer_.retrieve(n);
            // 如果Buffer可读空间已为空
            if(outputBuffer_.readableBytes() == 0){
                // 停止监听写事件，避免 busy-loop（一直触发 EPOLLOUT 但没有数据要发送）。
                channel_->disableWriting();
                // 触发用户注册的写完成回调
                if(writeCompleteCallback_){
                    // 保证回调在loop_所在线程执行；避免在别的线程执行，并发访问TcpConnection导致数据竞争
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                // 如果连接正在关闭，说明用户调用了shutdown()，但仍有数据未发送；
                // 现在数据已经发送完了，可以关闭socket了
                if(state_ == kDisconnecting){
                    // 在当前所属的loop中把TcpConnection删除掉
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else{
        LOG_ERROR("TcpConnection fd=%d is down, no more writing", channel_->fd());
    }
}

// 处理连接关闭的回调函数，当TCP连接 对端关闭 或者 异常断开 时，Poller检测到EPOLLHUP 或 EPOLLRDHUP事件，触发这个回调
void TcpConnection::handleClose(){
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d\n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    // 获取 TcpConnection 智能指针
    // TcpConnection 是 通过 std::shared_ptr 管理的。
    // 如果 TcpConnection 仅用 裸指针 this 传递，可能导致 回调执行过程中对象被释放，出现 野指针问题。
    // 通过 shared_from_this() 延长 TcpConnection 生命周期，直到回调函数执行完毕，保证安全。
    TcpConnectionPtr connPtr(shared_from_this());
    // 触发用户注册的连接回调
    connectionCallback_(connPtr);
    // 执行关闭连接的回调 最终清理连接资源。
    // closeCallback_ 是 TcpServer::removeConnection()，它的作用是：
    //    从 TcpServer 维护的连接池中移除当前连接。
    //    回收 TcpConnection 资源，防止内存泄漏。
    closeCallback_(connPtr);
}

// 触发EPOLLERR事件，EventLoop调用handleError()
void TcpConnection::handleError(){
    int optval;     // 存储获取到的socket错误码
    socklen_t optlen = sizeof optval;   // optval的大小，getsockopt需要这个参数
    int err = 0;    // 最终存储错误码

    /* int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
     * level = SOL_SOCKET：表示操作socket级别的选型
     * optname = SO_EEROR：表示获取socket的错误状态
     * optval：存储错误码的变量地址
     * optlen：optval变量大大小
     */
    // 在 epoll 触发 EPOLLERR 事件 时，不能直接通过 errno 获取 socket 的错误，而是要 查询 SO_ERROR 选项，才能拿到具体的错误原因。
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        // getsockopt返回附属表示调用失败，此时errno存储的是getsockopt()调用失败的错误代码
        err = errno;
    }
    else{
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name.c_str(), err);
}

// 用于向对端发送数据
void TcpConnection::send(const std::string &buf){
    // 处于kConnected才发数据
    if(state_ == kConnected){
        // loop_是TcpConnection所在的EvenLoop，说明是单Reactor模型，直接调用sendInLoop()发送数据
        if(loop_->isInLoopThread()){
            sendInLoop(buf.c_str(), buf.size());
        }
        // loop_在其他线程，则调用runInLoop()将任务投递到loop_线程里执行，保证sendInLoop()允许在loop_线程，防止并发访问TcpConnection导致数据竞争问题
        else{
            // 在 send() 里：
            //    直接用 this 是安全的，因为 TcpConnection 在 loop_ 里执行，生命周期通常不会被意外释放。
            //    不使用 shared_from_this() 可以避免额外的 shared_ptr 计数增加，减少 TcpConnection 被不必要地延长生命周期。
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
    // 250319 adangmmm's add
    else{
        LOG_ERROR("TcpConnection::send - not connected");
    }
}

// 实际执行数据发送的方法，在EventLoop线程中运行，并直接操作write系统调用或者使用缓冲区进行数据管理
// 发送数据 应用写的快 而内核发送数据慢 需要把待发送数据写入缓冲区，而且设置了水位回调
void TcpConnection::sendInLoop(const void *data, size_t len){
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if(state_ == kDisconnected){ 
        LOG_ERROR("disconnected, give up writing");
        return; //252319 adnagmmm's add
    }

    // 如果channel_之前没有在写，并且outputBuffer_没有待发的数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0){
        // 说明可以直接尝试写入socket，避免不必要的缓冲区操作，提升效率
        nwrote = ::write(channel_->fd(), data, len);
        // 写入成功
        if(nwrote >= 0){
            remaining = len - nwrote;   //还剩多少
            // 如果都写完了没剩，且用户注册的写完成回调函数存在
            if(remaining == 0 && writeCompleteCallback_){
                // 则放入loop_回调队列中，通知用户写入完成
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        // 写入失败
        else{
            nwrote = 0;
            // errno == EWOULDBLOCK：说明非阻塞情况下，写缓冲区满了，这种情况需要缓冲数据并注册 EPOLLOUT 事件（等待可写通知）。
            // 这里!=说明不正常返回了
            if(errno != EWOULDBLOCK){
                LOG_ERROR("TcpConnection::sendInLoop");
                // 说明对方关闭连接或者连接被重置了
                if(errno == EPIPI || errno == ECONNRESET){
                    faultError = true;  //后续不再发送
                }
            }
        }
    }
    /** 处理未写完的数据
     * 说明当前这一次write并没有把数据全部发送出去 剩余的数据需要保存到缓冲区当中
     * 然后给channel注册EPOLLOUT事件，Poller发现tcp的发送缓冲区有空间后会通知
     * 相应的sock->channel，调用channel对应注册的writeCallback_回调方法，
     * channel的writeCallback_实际上就是TcpConnection设置的handleWrite回调，
     * 把发送缓冲区outputBuffer_的内容全部发送完成
     **/ 
    if(!faultError && remaining > 0){
        // 当前ouputBuffer_中已有的可读(待发送)数据的大小
        size_t oldLen = outputBuffer_.readableBytes();
        // 判断条件1：如果追加的数据后，缓冲区总数据 ≥ 高水位，说明已经超过警戒线，可能需要提醒应用层。
        // 判断条件2：但原本的数据 oldLen 还没超过高水位，即：这次写入导致了数据量第一次超过阈值。
        //           确保 "高水位回调" 只在数据量第一次超过阈值时触发，而不是每次都触发。

        if(oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char *)data + nwrote, remaining);
        // 让 poller 监听可写事件，等内核缓冲区有空间时，通知 channel_，触发 handleWrite() 继续发送 outputBuffer_ 里的数据。
        if(!channel_->isWriting()){
            channel_->enableWriting();
        }
    }
}

void TcpConnection::sendFile(int fileDescriptor, off_t offset, size_t count){
    if(connected()){
        if(loop_->isInLoopThread){
            sendFileInLoop(fileDescriptor, offset, count);
        }
        else{
            // 这里用shared_from_this()还是this存疑，应该是个很细节的问题250319
            loop_->runInLoop(std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, count));
        }
    }
    else{
        LOG_ERROR("TcpConnection::sendFile - not connected");
    }
}

// kamma自己写的
void TcpConnection:sendFileInLoop(int fileDescriptor, off_t offset, size_t count){
    ssize_t bytesSent = 0;      // 已经发送了多少了
    size_t remaining = count;   // 还剩多少没发
    bool faultError = false;

    if(state_ == kDisconnected){
        LOG_ERROR("disconnected, give up writing");
        return;
    }

    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0){
        // 从 fileDescriptor 中读取数据，并通过 socket_->fd() 发送到网络上。
        // 将文件内容直接拷贝到套接字的发送缓冲区，避免了在用户空间和内核空间之间的额外内存拷贝。
        // 为什么用socket_而非channel_，存疑250319
        byetsSent = sendfiles(socket_->fd(), fileDescriptor, &offset, remaining);
        if(bytesSent >= 0){
            remaining = count - bytesSent;
            if(remaining == 0 && writeCompleteCallback_){
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else{
            if(errno != EWOULDBLOCK){
                LOG_ERROR("TcpConecction::sendFileInLoop");
                if(error == EPIPE || error == ECONNRESET){
                    faultError = ture;
                }
            }
        }
    }

    // 处理剩余数据
    if(!faultError && remaining > 0){
        loop_->queueInLoop(std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, count));
    }
}

void TcpConnection:shutdown(){
    if(state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop(){
    if(!channel_->iswriting()){
        socket_->shutdownWrite();
    }
}


// 建立连接
void TcpConnection::connectEstablished(){
    // 设置状态为已连接
    setState(kConnected);
    // 绑定生命周期，确保channel在TcpConnection销毁前销毁
    channel_->tie(shared_from_this());
    // 让poller监听EPOLLIN事件
    channel_->enableReading();

    // 建立连接，执行用户注册的回调
    connectionCallback_(shared_from_this());
}
// 连接销毁
void TcpConnection::connectDestroyed(){
    // 只有当连接是 已连接状态 才需要清理。
    // 防止重复销毁（例如异常关闭后 state_ 可能已经变了）。
    if(state_ == kConnected){
        setState(kDisconnected);
        // 取消 poller 对 channel_ 的所有监听。避免 TcpConnection 销毁后，channel_ 仍然收到事件，导致野指针访问。
        channel_->disableAll();
        // 执行 用户的关闭回调，通知上层应用 连接已关闭，可以进行资源清理。
        // 调用相同的回调 connectionCallback_，但由于 TcpConnection 状态不同（kConnected vs kDisconnected），用户可以在回调函数内根据 TcpConnection 当前状态执行不同逻辑。
        connectionCallback_(share_from_this());
    }
    // 彻底把 channel_ 从 poller 中移除，确保不再监听任何事件。
    channel_->remove();
}