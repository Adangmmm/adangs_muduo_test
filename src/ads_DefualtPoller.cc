#include <stdlib.h>

#include "ads_Poller.h"
#include "ads_EPollPoller.h"

Poller *Poller::newDefualtPoller(EventLoop *loop){
    if(::getenv("MUDUO_USE_POLL")){
        // 如果环境变量 MUDUO_USE_POLL 存在：
        // 说明用户想要使用 poll 而不是 epoll
        // 但是 poll 版本未实现（返回 nullptr，表示当前代码不支持 poll）
        return nullptr;
    }
    else{
        // 如果环境变量 MUDUO_USE_POLL 不存在：
        // 默认创建 epoll 版本的 Poller
        // new EPollPoller(loop); 创建 epoll 实例
        return new EPollPoller(loop);
    }
}

/* 为什么 newDefaultPoller() 单独放在 DefaultPoller.cc？
    1.解耦 Poller.cc 和具体实现 EPollPoller，避免 Poller.cc 依赖 epoll
    2.便于未来扩展 poll、kqueue 等新 Poller，只需要修改 DefaultPoller.cc
    3.减少编译依赖，提高编译速度，避免 Poller.cc 变更导致 EPollPoller 重新编译
    4.遵循“一文件一职责”原则，让 Poller.cc 仅关注 Poller 抽象逻辑
    5.避免循环依赖问题，让 Poller.h 不依赖 EPollPoller.h
*/