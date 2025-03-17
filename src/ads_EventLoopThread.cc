#include "ads_EventLoopThread.h"
#include "ads_EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string %name)
    : loop_(nullptr)
    , exiting_(false)
    // this 是指向当前 EventLoopThread 实例的指针
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{
}

EventLoopThread::~EventLoopThread(){
    exiting = true;
    // 检查 loop_ 是否已经初始化，如果 nullptr 说明 EventLoop 没有创建，直接返回。
    if(loop_ != nullptr){
        loop_->quit();
        // 等待线程结束，确保 EventLoopThread 线程安全地退出。
        // join() 会阻塞当前线程，直到 thread_ 执行完 threadFunc() 并完全退出。
        thread_.join();
    }    
}

EventLoop *EventLoopThread::startLoop(){
    // 启动新线程，执行 threadFunc()。
    thread_.start();

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex);
        // 释放 mutex_，阻塞当前线程（主线程），直到满足 loop_ != nullptr 条件。
        // 等待cond_.notify_one() 唤醒主线程。被唤醒后主线程会重新获得mutex_，继续执行
        cond_.wait(lock, [this]()<return loop_ != nullptr;>);
        loop = loop_;
    }
    return loop;
}
/*
主线程                                  子线程（threadFunc）
------------------------------------------------------
EventLoopThread::startLoop()       
  |
  |--> thread_.start()  ------------>  threadFunc()  (创建新线程)
  |                                     |
  |                                     |--> EventLoop loop;
  |                                     |
  |                                     |--> loop_ = &loop;
  |                                     |--> cond_.notify_one();
  |                                     |--> loop.loop();
  |
  | <-- (被唤醒) cond_.wait(lock);
  |--> 返回 loop_
  |
  |--> 继续使用 loop_
*/


// 下面这个方法 是在单独的新线程里运行的
void EventLoopThread::threadFunc(){
    // 在新线程中创建一个 EventLoop 对象，但它会在当前线程内运行，这符合one loop per thread 的设计。
    EventLoop loop;

    // callback_ 是一个 std::function<void(EventLoop *)> 类型的回调函数，用户可以在 EventLoop 线程启动时执行一些额外的初始化逻辑。
    // 如果 callback_ 不为空，就执行它，并传入 loop 指针，使得用户可以在 EventLoop 运行前进行配置。
    if(callback_){
        callback_(&loop)
    }

    // 加锁，确保 loop_ = &loop; 这一赋值操作是线程安全的。
    {
        std::unique_lock<std::mutex> lock(mutex);
        // 让主线程拿到 EventLoop 指针
        loop_ = &loop;
        // 唤醒正在等待 loop_ 赋值的主线程。
        cond_.notify_one();
    }
    // 启动事件循环，让 EventLoop 开始监听 I/O 事件、执行定时任务、运行回调等。
    loop.loop();

    std::unique_lock<std::mutex> lock(mutex);
    // loop_ 只是一个指向 EventLoop 的指针，并不负责管理 EventLoop 的生命周期。
    // 当 loop.loop() 退出时，意味着 EventLoop 已经销毁，所以将 loop_ 置空，防止主线程访问一个无效的指针。
    loop_ = nullptr;

/**
    主线程       线程 1（threadFunc）
--------------------------------------------------
    EventLoopThread.startLoop()
    | 
    |--> 启动线程 threadFunc()
                |
                |--> 创建 EventLoop loop
                |--> 赋值 loop_ = &loop
                |--> 通知主线程 cond_.notify_one()
    |
    |<-- 主线程被唤醒，获取 loop_
    |
    |--> 继续执行主线程逻辑
                |
                |--> loop.loop() 开始事件循环
 */

}
