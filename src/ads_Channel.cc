#include <sys/epoll.h>

#include "ads_Channel.h"

//250310 还没写
//#include "ads_EventLoop.h"
//#include "ads_Logger.h"

const int Channel::kNoneEvent = 0;  //空事件
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; //读事件
const int Channel::kWriteEvent = EPOLLOUT;  //写事件

//
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd(fd_)
    , events(0)
    , revents(0)
    , index_(-1)
    , tied_(false)
{    
}

Channel::~Channel()
{
}

// 
/** 
 * TcpConnection中注册了Channel对应的回调函数，传入的回调函数均为TcpConnection对象
 * 的成员方法，所以Channel的结束一定晚于TcpConnection对象。
 * 用tie解决TcpConnection和Channel的生命周期时常问题，保证Channel对象
 * 在TcpConeection对象销毁前销毁
 **/
void Channel::tie(const std::shared_ptr<void &obj>){
    tie_ = obj;
    tied_ = true;   //  绑定完标志为true
}

// update 和 remove => EpollPoller 一起更新Channel在poller中的状态
/**
 * Channel所表示的fd中的events事件改变后，update负责在Poller里对应的事件epoll_ctl
 **/
void Channel::update(){
    // Channel所属的EventLoop，调用其中Poller的对应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在Channel所属的EventLoop中把当前Channel移除
void Channel::remove(){
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        std::shared_ptr<void> gurd = tie_.lock();
        // 绑上了，就通过.lock()检查TcpConnection是否存活，存活就处理，没存活就buchuli
        if(gurd){
            handleEventWithGuard(receiveTime);
        }
    }
    else{
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime){
    LOG_INFO("channel handleEvent revent:%d\n", revents_)

    // 语法： &是按位与运算，revents_ & EPOLLHUP表示判断revents_是否包含EPOLLHUP事件
    //       if(closeCallback_)检查回调函数是否有效
    // 关闭
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){
        //EPOLLHUP表示fd被挂起，当TcpConnection对应Channel 通过shutdown 关闭写端 epoll触发EPOLLHUP
        if(closeCallback_){
            closeCallback_();
        }
    }
    // 错误
    if(revents_ & EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }
    // 读
    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }
    // 写
    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}