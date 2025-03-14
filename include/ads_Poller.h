#pragma once

#include <vector>
#include <unordered_map>

#include "ads_noncopyable.h"
#include "ads_Timestamp.h"

class Channel;
class EventLoop;

/** muduo库中多路事件分发器的核心IO复用模块
 * Poller负责监听文件描述符fd事件是否触发 以及 返回发生事件的文件描述符fd和具体事件。
 * 
 * Poller是个抽象虚类，由EpollPoller和PollPoller继承实现，监听fd和返回监听结果的具体方法在派生类中实现
 * EpollPoller就是封装了epoll方法实现的的与事件监听有关的各种方法；同理PollPoller
 * **/ 
class Poller
{
public:
    using ChannelList = std::vector<Channel *>; 

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断channel是否在当前的Poller中
    bool hasChannel(Channel *channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // 哈希表 key:sockfd  value:sockfd所属的Channel对象的指针
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_;  //定义Poller所属的事件循环

};

