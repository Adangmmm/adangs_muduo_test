#pragma once

#include <functional>
#include <memory>


// 250310 还没写这俩
//#include "ads_noncopyable.h"
//#include "ads_Timestamp.h"

// 前置声明 EventLoop 类，告诉编译器这个类在后面会被使用，但不需要知道其具体定义。
// 这种做法可以减少不必要的头文件依赖，降低编译时间，增强封装性。
class EventLoop;

class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;    //C++11代替typedef
    using ReadEventCallback = std::function<void(Timestamp)>

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // 防止channel被手动REMOVE掉 channe还在执行回调操作（250310没理解）
    // 绑定TcpConnetction，确保Channel在TcpConnection销毁前已销毁，避免执行回调函数导致野指针
    void tie(const std::shared_ptr<void> &);
    
    void remove();

    // 设置回调函数
    void setReadCallback(ReadEventCallback cb) {readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) {closeCallback_ =  std::move(cb);}
    void setErrorCallback(EventCallback cb) {errorCallback_ = std::move(cb);}

    // fd得到Poller通知后处理事件，handleEvent在EventLoop::loop()中被调用
    void handleEvent(Timestamp receiveTime);

    int fd() const {return fd_;}
    int events() const {return events_;}
    int set_revents(int revt) {revens_ = revt;}

    // 设置fd相应的事件状态，相当于epoll_ctl add delete
    void enableReading() {events_ |= kReadEvent; update();}
    void disableReading() {events_ &= ~kReadEvent; update();}
    void enableWriting() {events_ |= kWriteEvent; update();}
    void disableWriting() {events_ &= ~kWriteEvent; update();}
    void disableALL() {events_ = kNoneEvent; update();}

    // 返回fd当前事件的状态
    bool isNoneEvent() const {return events_ == kNoneEvent;}
    bool isReading() const {return events_ & kReadEvent;}
    bool isWriting() const {return events_ & kWriteEvent;}

    int index() {return index_;}
    void set_index(int idx) {index_ = idx;}

    // one loop per thread
    EventLoop *ownerLoop() {return loop_};
private:

    void update();  // 调用了epoll_ctl()
    void handleEventWithGuard(Timestamp receiveTiome);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;    // 事件循环
    const int fd_;      // 事件描述符fd，Poller监听的对象
    int events_;        // 注册fd感兴趣的事件
    int revents_;       // Poller(epoll_wait())返回的具体发生的事件，一个整数如0b1010
    int index_;         // 250310不懂

    std::weak_ptr<void> tie_;   // 弱引用（不增加引用次数，防止循环引用）TcpConnection，防止悬垂指针
    bool tied_;

    // Channel是个fd管家，封装了具体事件的回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};

