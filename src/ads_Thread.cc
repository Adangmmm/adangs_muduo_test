#include <semaphore.h>

#include "ads_Thread.h"
#include "ads_CurrentThread.h"

std::atomic_int Thread::numCreadted_(0);

Thread::Thread(ThreadFunc func, const std:string &name)
    : stated_(false)
    , joined_(false)
    , tid_(0)
    // std::move() 将 func 变成右值，触发 std::function 的移动构造。C++11
    // 如果直接使用 func_ = func，将执行拷贝操作，可能涉及动态内存分配，影响性能。
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread(){
    // 如果线程已启动未join
    if(started_ && !joined_){
        // thread_是一个std::shared_ptr<std::thread>，指向实际的std::thread对象
        // detach()作用是将线程与std::thread对象分离
        //     线程会在后台继续运行;线程结束后资源会自动回收
        //     是非阻塞的，调用detach()后，调用线程不会等待被分离线程的结束
        thread_->detach();
        // 如果在线程对象被销毁前未调用 join() 或 detach()，程序会触发 std::terminate()，导致程序崩溃。
    }
}

void Thread::start(){
    started_ = true;
    // 定义信号量，sem_t 是 POSIX 的信号量类型。
    semt_ sem;
    // false：表示信号量不在进程间共享，只在当前线程或当前进程内部使用。0：信号量的初始值为 0（表示等待状态，用于同步操作）。
    sem_init(&sem, false, 0);
    // 新线程的执行体是一个 Lambda 表达式：
    //   [&]：Lambda 捕获列表，按引用捕获外部变量。
    //   tid_ = CurrentThread::tid();：返回当前线程的系统级 ID。通过 tid_ 记录新线程的 ID。
    //   sem_post(&sem)：V操作，解除 sem_wait() 的阻塞。表示新线程已成功获取 tid。
    //   func_()：执行用户定义的回调函数（在新线程中执行）。
    thread = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_();
    }));
    // P操作，这里必须等待获取上面新创建的线程的tid值
    sem_wait(&sem);
}

void Thread::join(){
    joined_ = true;
    // std::thread::join() 的作用是：
    //    等待当前线程完成。
    //    如果线程已结束，join() 会立即返回。
    //    如果线程尚未结束，join() 会阻塞调用线程，直到线程执行完成。
    thread_->join();
}


void Thread::setDefaultName(){
    int num = ++numCreadted_;

    if(name_.empty()){
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        // 将 buf 转换为 std::string，并赋值给 name_。std::string 会自动进行内存管理，复制 buf 的内容。
        name_ = buf;
    }
}