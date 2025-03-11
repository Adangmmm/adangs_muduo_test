#include "Poller.h"
#include "Channel.h"

Poller:Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

bool Poller::hasChannel(Channel *channel) const{
    // it存储find()返回值，fd存在返回指向该元素的迭代器，fd不存在返回channels_.end()
    auto it = channels_.find(channel->fd());
    // 前半部分表示存在fd，若存在后半部分则确保channel* 和 fd匹配
    // 后半部分用于防止多个channel共享相同fd的问题，fd存在但可能指向一个已经销毁的Channel*，所以要匹配
    return it != channels_.end() && it->second == channel;
}

