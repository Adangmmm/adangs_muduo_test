#pragma onece

#include <vector>
#include <sys/epoll.h>

#include "ads_Poller.h"
// 250311还没写
//#include "ads_Timestamp.h"

/**
 * epoll的使用：
 * 1. epoll_create
 * 2. epoll_ctl(add, mod, del)
 * 3. epoll_wait 等待指定的fd上的事件发生
 **/

 class Channel;

 class EPollPoller : public EPollPoller
 {
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 重写基类Poller的抽象方法
    TImestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    // 初始化 vector<epoll_event>(16)，即分配 16 个 epoll_event 事件空间。
    static const int kInitEventListSize = 16;

    // 填写活跃的channels
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道，其实就是调用epoll_ctl
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;       // epoll_create创建返回的fd保存在epollfd_中
    EventList events_;  // 用于存放epoll_wait返回的所有发生的事件的文件描述符事件集

 };