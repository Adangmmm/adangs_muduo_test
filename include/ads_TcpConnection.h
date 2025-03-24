#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "ads_noncopyable.h"
#include "ads_InetAddress.h"
#include "ads_Callbacks.h"
#include "ads_Buffer.h"
#include "ads_Timestamp.h"

class Channel;
class EventLoop;
class Socket;


// 如果TcpConnection对象是由shared_prt<TcpConnection>管理的，那么在类的成员函数内部，如果想要获取一个只想自身的shared_ptr<TcpConnection>
// 不能直接用this创建，否则会导致多个shared_ptr共享原始指针，从而导致引用技术错误，可能导致对象提前释放或内存泄漏
// 提供 shared_from_this()，在类内部安全地获取 shared_ptr。
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                  const std::string &nameArg,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const {return loop_;}
    const std::string &name() const {return name_;}
    const InetAddress &localAddress() const {return localAddr_;}
    const InetAddress &peerAddress() const {return peerAddr_;} 

    bool connected() const {return state_ == kConnected;}

    // 发送数据，非阻塞
    // ，const void *data 允许 sendInLoop 函数接收 任何类型的指针，并且 不修改数据（const 关键字表明数据不可修改）。
    void send(const std::string &buf);
    // 零拷贝发送文件
    void sendFile(int fileDescriptor, off_t offset, size_t count);
    
    // 半关闭，只关闭写端
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb;}
    void setMessageCallback(const MessageCallback &cb) {messageCallback_ = cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) {writeCompleteCallback_ = cb;}
    void setCloseCallback(const CloseCallback &cb) {closeCallback_ = cb;}
    void setHightWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark;}

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

private:
    // 定义了一个枚举类型，里面的类型会默认对应0，1，2，3... 也可以显示指定 = value
    enum StateE{
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting,
    };
    void setState(StateE state) {state_ = state;}

    // 可读事件，调用messageCallbak_
    void handleRead(Timestamp receiveTime);
    void handleWrite();     // 写事件，发送缓冲区数据
    void handleClose();     // 连接关闭
    void handleError();     // 错误处理

    void sendInLoop(const void *data, size_t len);
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);
    void shutdownInLoop();

    // TcpServer中，若为单Reactor程则loop_为baseloop，若为多Reactor则loop_为subloop
    EventLoop *loop_;
    const std::string name_;
    std::atomic_int state_;
    // 连接是否在监听读事件
    bool reading_;

    // 管理底层socket和epoll，与Acceptor类似    Acceptor => mainloop    TcpConnection => subloop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 这些回调，用户通过写入TcpServer注册，TcpServer再将注册的回调传给TcpConnection，TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;         // 有新连接时的回调
    MessageCallback messageCallback_;               // 有新读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;  // 消息发送完成后的回调
    HighWaterMarkCallback highWaterMarkCallback_;   // 高水位回调
    CloseCallback closeCallback_;                   // 关闭连接的回调
    size_t highWaterMark_;     //高水位阈值

    // 数据缓冲区
    Buffer inputBuffer_;
    Buffer outputBuffer_;

};