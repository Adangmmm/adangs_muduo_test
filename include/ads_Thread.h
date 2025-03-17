#pragma once

#include <functional>   // 提供std::function，用于存储可调用对象
#include <thread>       // 提供std::thread，用于创建和管理线程
#include <memory>       // 提供std::shared_ptr，实现线程对象共享管理
#include <unistd.h>     // 提供getpid()等POSIX系统调用接口
#include <string>
#include <atomic>       // 提供std:atomic，用于线程安全的原子操作

#include "noncopyable.h"

// 将std::thread 进行了功能性增强和封装，满足了高性能网络库的需求
class Thread : noncopyable{
public:
    // 定义一个函数对象类型，用于表示线程要执行的回调函数
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    // 创建并启动线程，下哟调用std::thread构造函数来创建线程并执行func_
    void start();
    // 等待线程结束，下哟调用std::thread::join()来等待线程结束，防止线程成为孤儿线程or未清理线程
    void join();

    bool started() {return started_;}
    pid_t tid() const {return tid_;}
    const std::string &name() {return name_;}

    static in numCreadted() {return numCreated_;}

private:
    //设置默认线程名
    void setDefaultName();

    bool started_;  // 标识线程是否已启动
    bool joined_;   // 标识线程是否已join

    std::shared_ptr<std::thread> thread_;   // 线程对象的智能指针
    pid_t tid_;     // 线程系统级ID
    ThreadFunc func_;   // 线程执行的函数
    std::string name_;  // 线程名
    static std::atomic_int numCreadted_;    // 已创建的线程总数
}