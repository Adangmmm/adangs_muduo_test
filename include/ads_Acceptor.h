#pragma once

#include <functional>

// #include <ads_noncopyable.h>
// #include "ads_Socket.h"
#include "ads_Channel.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable{
public:
    // 定义了一个回调函数的类型，表示在有新连接时，回调函数的签名是: sockfd表示新连接的socket描述符，InetAddress表示新连接的地址信息
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

    // loop: 事件循环对象指针，表示在那个循环里面监听
    // listenAddr: 要监听的地址和端口
    // reuseport: 是否开启端口复用, 是否开启SO_REUSEPORT选项, 表示多个socket可以绑定到同一个端口，用于负载均衡，提高服务器的性能
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    // 设置新连接到来时的回调函数, 将回调函数存储在newConnectionCallback_中
    void setNewConnectionCallback(const NewConnectionCallback &cb){NewConnectionCallback_ = cb;}
    // 是否在监听
    bool listenning() const {return listenning_;}
    // 调用底层socket接口，开始监听
    void listen();

private:

    void handleRead();  //当有新连接时，EventLoop会调用这个方法处理新连接

    EventLoop *loop_;   //事件循环对象指针，将Acceptor纳入到EventLoop中管理
    Socket accpetSocket_;   //封装了一个sockt用于接收新连接
    Channel acceptChannel_; //将acceptSocket_封装成一个Channel，用于监听新连接事件
    NewConnectionCallback NewConnectionCallback_; //储存新连接到来时的回调函数
    bool listenning_;  //判断是否正在监听
}