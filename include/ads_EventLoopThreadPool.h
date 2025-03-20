#pragma ocne

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "ads_noncopyable.h"
// 250320 发现问题
// #include "ads_ConsistenHash.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable{
public:
    using ThreadInitCallback = std::functional<void(EventLoop *)>;

    // baseLoop → 主线程中的 EventLoop（若线程数 numThreads_ == 1，直接使用 baseLoop_）。
    // 线程池需要绑定一个baseLoop，主线程的baseloop用于监听连接，多线程时baseLoop接收连接并分发给子线程的EventLoop处理
    // nameArg → 线程池的名称（给 EventLoopThread 命名）。
    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) {numThreads_ = numThreads;}

    // 启动线程池，创建多个 EventLoopThread。
    void start(consst ThreadInitCallback &cb = ThreadInitCallback();)

    // 获取下一个EventLoop进行负载均衡，若是多线程，会轮询/一致性哈希分配任务
    // 250320 发现错误
    // EventLoop *getNextLoop(const std::string &key);

    // 获取所有EventLoop指针
    std::vector<EventLoop *> getAllLoops();

    bool started() const {return started_;}
    const std::string name() const {return name_;}

private:
    EventLoop *baseLoop_;
    // 存储所有EventLoopThread线程，用std::unique_ptr管理
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    // 存储所有EventLoop 指针 
    std::vector<EventLoop *> loops_;

    // getNextLoop()轮询时的索引
    int next_;
    // 用于基于key选择EventLoop
    // 250320 发现问题
    // ConsistenHash hash_;

    // 线程池名称，通常由用户指定，池中EventLoopThread的名称依赖于线程池的名称
    std::string name_;
    bool started_;
    int numThreads_;
};