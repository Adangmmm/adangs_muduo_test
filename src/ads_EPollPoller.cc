#include "errno.h"
#include "unistd.h"
#include "string.h"

#include "ads_EPollPoller.h"
// 250311还没写
//#include "ads_Logger.h"
#include "ads_Channel.h"

const int kNew = -1;    // 某个channel还没添加至Poller // channel的成员index_初始化为-1
const int kAdded = 1;   // 某个channel已经添加至Poller
const int kDeleted = 2; // 某个channel已经从Poller中移除

EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop)
    // 创建一个epoll示例，返回epoll的文件描述符epollfd_，EPOLL_CLOEXEC作用是子进程不会继承这个epollfd_，防止文件描述符泄露
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize) // 初始化 vector<epoll_event>(16)，即分配 16 个 epoll_event 事件空间。
{
    if(epollfd_ < 0){
        // epoll_create1() 失败时，返回 -1，errno 记录错误码，LOG_FATAL 记录日志并终止程序。
        LOG_FATAL("epoll_create error:%d/n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannnels){
    // —__FUNCTION__是一个预定义宏，表示当前函数名称，channels_.size()表示当前epoll监听的fd数量 
    LOG_INFO("func=%s +> fd total count:%lu\n", __FUNCTION__, channels_.size())

    /*
    * &*events_.begin() 表示传入存储触发事件的数组
    * events_.size() 表示监听事件的最大数量
    * timeoutMs 表示超时时间，-1表示无限等待
    * 整个numEvents可能返回三个结果：>0表示触发的事件数量；=0表示超时即无事发生；<0表示发生错误
    */
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cat<int>(events_.size()), timeoutMs);

    // errno是全局变量，表示最近一次系统调用的错误码，调用epoll_wait()可能会修改errno，所以先保存下来
    int saveErrno = errno;
    Timestamp now(Timestamp::now());    //Timestamp::now记录当前时间用于返回给上层，表示触发事件的时间戳

    if(numEvents > 0){
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannnels);
        // 如果触发的事件数量等于当前events_大小，说明事件列表容量不足，要进行扩容
        if(numEvents == events_.size()){
            events_.resize(events_.size() * 2);
        }
    }
    else if(numEvents == 0){
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else{
        //errno 若为EINTR说明被信号中断
        if(saveErrno != EINTR){
            errno = saveErrno;
            LOG_ERROR("EPollPoller:poll() error!");
        }
    }

    return now;
}

// channel::update remove => EventLoop::updateChannel remove Channel => Poller::updateChannel removeChannel
viod EPollPoller:updateChannel(Channel *channel){

    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd, channel.events(), index);

    if(index == kNew || index == kDeleted){
        if(index == kNew){
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        else{   //index == kDeleted       
                // channel仍存在于channels_，但处于禁用状态，只需要update(EPOLL_CTL_ADD)重新将其添加到epoll就好了
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else{   //channel已经在Poller中注册过了
        int fd = channel->fd();
        if(channel->isNoneEvent()){ //如果返回true即该channel确实没有感兴趣的事件，则从epoll中移除
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel *channel){
    int fd = channel->fd();
    // epoll 本质上是基于 fd 的操作，channels_ 仅仅是用户空间的记录结构。
    // epoll_ctl() 通过 fd 在内核级别管理事件,删除 channels_ 中的 fd，代表这个 Channel 在用户空间已被移除
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if(index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}


/** 填写活跃的连接
 * 1.遍历 events_ 数组中的 numEvents 个事件：
 *   events_[i] 是 epoll_wait() 监听到的一个事件。
 * 2.找到事件对应的 Channel
 *   events_[i].data.ptr 是 存储的 Channel* 指针（注册 epoll 事件时存的）。
 *   static_cast<Channel *> 进行类型转换，得到 Channel*。
 * 3.设置 Channel 发生的事件类型
 *   events_[i].events 是 具体的 epoll 事件类型（如 EPOLLIN、EPOLLOUT 等）。
 *   channel->set_revents(events_[i].events); 让 Channel 记录 它发生了哪些事件。
 * 4.把 Channel 加入 activeChannels
 *   activeChannels->push_back(channel); → 让 EventLoop 知道有哪些 Channel 需要处理。
 */
void EPollPoller:fillActiveChannels(int numEvents, ChannelList *activeChannels) const{
    for(int i = 0; i < numEvents; ++i){
        // events_[i].data.ptr 是 void* 类型（epoll_event 结构体定义）。static_cast<Channel *> 用于把 void* 转换成 Channel* 指针。
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

// 更新channel通道， 其实就是调用epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel){
    epoll_event event;
    // :: 表示 调用全局 memset，避免和 class 里的 memset 发生冲突。作用是 将 event 清零，防止脏数据影响 epoll 监听。
    ::memset(&event, 0, sizeof(event));

    int fd = channel->fd;

    event.events = channel->events;
    event.data.fd = fd;
    event.data.ptr = channel;

    // epoll_ctl() 返回 负值 表示出错，打印日志：
    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else{
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}