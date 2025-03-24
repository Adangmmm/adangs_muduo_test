#pragma once

#include <functional>
// <mutex> 和 <condition_variable>提供线程同步机制
#include <mutex>
#include <condition_variable>
#include <string>

#include "ads_noncopyable.h"
#include "ads_Thread.h"

class EventLoop;

class EventLoopThread : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    // = 用于默认参数，表示如果调用构造函数时未提供 cb，则默认使用 ThreadInitCallback()（即空回调）。
    // ThreadInitCallback() 创建一个默认的 std::function<void(EventLoop *)>，即空函数对象。
    // 这样 callback_ 不会执行任何操作，除非用户显式传递了回调函数。
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();


private:
    //线程的入口函数，在线程启动后执行。负责创建EventLoop，并进入其事件循环。loop_初始化完成后，使用条件变量通知startLoop()
    void threadFunc();

    EventLoop *loop_;           // 指向线程中的EventLoop
    bool exiting_;              // 线程退出标志
    Thread thread_;             // 线程对象
    // 互斥锁，用于保护loop_共享数据空间
    std::mutex mutex_;
    // 条件变量，用于startLoop()等待EventLoop初始化完成
    std::condition_variable cond_;
    // 线程初始化回调，在EventLoop启动后执行额外的初始化逻辑
    ThreadInitCallback callback_;
};