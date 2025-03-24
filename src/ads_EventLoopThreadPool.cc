#include <memory>

#include "ads_EventLoopThreadPool.h"
#include "ads_EventLoopThread.h"
#include "ads_Logger.h"


// 初始化 EventLoopThreadPool，但不会创建 EventLoopThread 线程。
// start() 之后，才会真正创建多个 EventLoopThread 并启动它们。
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
    // 一致性哈希对象，默认3个虚拟点
    // 删掉了
    //, hash_(3)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // baseLoop_ 不是 new 创建的，不需要手动释放，它是用户代码中的局部变量。
    // 线程池管理的 EventLoopThread 是 unique_ptr，在 threads_ 容器析构时自动释放，无需手动 delete。
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb){
    started_ = true;

    for(int i = 0; i < numThreads_; ++i){
        char buf[name_.size() + 32];
        // name_ + int 需要sizeof(buf) + 32bits
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        // EventLoopThread::startLoop返回一个EventLoop *
        loops_.push_back(t->startLoop());
        // hash_.addNode(buf);：将每个线程的名称（如 "WorkerPool0", "WorkerPool1", ...）
        // 作为节点添加到一致性哈希中。这使得后续的请求可以根据一致性哈希分配到特定的 EventLoop。
        // 删掉了
        // hash_.addNote(buf);
    }

    // numThreads_ == 0：当没有额外的 EventLoopThread 时，线程池只有主线程的 baseLoop_。这种情况下，baseLoop_ 用于处理所有的 IO 事件。
    // cb(baseLoop_);：如果提供了回调 cb，则直接执行回调函数，回调函数会在主线程中的 baseLoop_ 上执行一些初始化工作。
    if(numThreads_ == 0 && cb){
        cb(baseLoop_);
    }
}

// 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
/* 250320 发现错误，这里调用了getNode()但getNode()返回的是节点对应的哈希值，无法作为索引返回loops_[index]
EventLoop *EventLoopThreadPool::getNextLoop(const std::string &key){
    size_t index = hash_.getNode(key);
    if(index >= loops_.size()){
        LOG_ERROR("EventLoopThreadPool::getNextLoop ERROR");
        return baseLoop_;
    }
    return loops_[index];
}
*/
// 若工作中多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
EventLoop *EventLoopThreadPool::getNextLoop(){
    // 如果只设置了一个线程，则getNextLoop()每次都返回当前的baseLoop_
    EventLoop *loop = baseLoop_;

    // 通过轮询获取下个处理事件的loop，没设置多线程数量则不会进去
    if(!loops_.empty()){
        loop = loops_[next_];
        ++next_;
        // 轮询
        if(next_ >= loops_.size()){
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops(){
    if(loops_.empty()){
        // 如果 loops_ 为空，则返回一个包含单一元素 baseLoop_ 的 std::vector。
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    else{
        return loops_;
    }
}

