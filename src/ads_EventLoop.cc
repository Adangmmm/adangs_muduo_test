#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

#include "ads_EventLoop.h"
#include "ads_Logger.h"
#include "ads_Channel.h"
#include "ads_Poller.h"

// __thread 是 GCC 和 Clang 支持的线程局部存储（TLS）机制,每个线程都有一个独立的 t_loopInThisThread 变量，互不干扰。
// 通过 __thread 限制每个线程只能拥有一个 EventLoop 实例。one loop per thread
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间 10s
const int kPollTimeMs = 10000;

/* 创建线程之后主线程和子线程谁先运行是不确定的。
 * 通过一个eventfd在线程之间传递数据的好处是多个线程无需上锁就可以实现同步。
 * eventfd 是 Linux 提供的用于线程或进程间通信的机制。它创建了一个文件描述符，允许通过写入和读取进行事件通知
 * eventfd支持的最低内核版本为Linux 2.6.27,在2.6.26及之前的版本也可以使用eventfd，但是flags必须设置为0。
 * 函数原型：
 *     #include <sys/eventfd.h>
 *     int eventfd(unsigned int initval, int flags);
 * 参数说明：
 *      initval,初始化计数器的值。
 *      flags, EFD_NONBLOCK,设置socket为非阻塞。
 *             EFD_CLOEXEC，执行fork的时候，在父进程中的描述符会自动关闭，子进程中的描述符保留。
 * 场景：
 *     eventfd可以用于同一个进程之中的线程之间的通信。
 *     eventfd还可以用于同亲缘关系的进程之间的通信。
 *     eventfd用于不同亲缘关系的进程之间通信的话需要把eventfd放在几个进程共享的共享内存中（没有测试过）。
 */
// 创建wakeupfd 用来notify唤醒subReactor处理新来的channel
int createEventfd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0){
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())                    //创建 eventfd，用于跨线程通信
    , wakeupChannel_(new Channel(this, wakeupFd_))  //封装 eventfd，便于在 Poller 中监听事件
{
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    // 如果当前线程已存在 EventLoop，直接报错退出。如果当前线程没有 EventLoop，将当前对象记录在 t_loopInThisThread。
    // 保证每个线程最多只能存在一个 EventLoop。
    if(t_loopInThisThread){
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else{
        t_loopInThisThread = this;
    }
    // wakeupChannel_ 是对 eventfd 的封装。绑定读事件的回调函数为 handleRead()。handleRead() 在 eventfd 上有数据可读时触发
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 将 wakeupFd_ 添加到 Poller 中，监听其读事件。Poller 在 epoll_wait() 发现 eventfd 可读时，触发 handleRead()
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop(){
    // 调用disableAll()让wakeupChannel_停止监听所有事件（不再关注任何事件）
    wakeupChannel_->disableALL();
    // 调用remove()将wakeupChannel_从Poller中移除
    wakeupChannel_->remove();
    // 关闭eventfd文件描述符，防止资源泄露
    ::close(wakeupFd_);
    //  避免悬空指针
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p star looping\n", this);

    while(!quit_){
        // 每次循环开始前，清空上一轮出发的事件列表。防止脏数据干扰本轮事件处理
        activeChannels_.clear();
        // 等待内核返回已触发的IO事件，超时事件为10s
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        // 遍历所有活跃Channel
        for(Channel *channel : activeChannels_){
            // 调用channel->handleEvent()处理具体事件
            channel->handleEvent(pollReturnTime_);
        }
        // 调用doPendingFunctors()执行其他线程提交的异步任务
        /**
         * 执行当前EventLoop事件循环需要处理的回调操作 对于线程数 >=2 的情况 IO线程 mainloop(mainReactor) 主要工作：
         * accept接收连接 => 将accept返回的connfd打包为Channel => TcpServer::newConnection通过轮询将TcpConnection对象分配给subloop处理
         * 
         * mainloop调用queueInLoop将回调加入subloop
         * 该回调需要subloop执行 但subloop还在poller_->poll处阻塞, queueInLoop通过wakeup将subloop唤醒
         * wakeup()通过eventfd触发事件，促使epoll_wait()立即返回
         **/
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping.\n", this);
    looping_ = false;
}

/**
 * 如果在当前线程中调用quit()，则循环会在本次执行完毕loop()中的poller_->poll()后自然退出
 * 如果在其他线程中调用quit()，则需要通过eventfd触发唤醒，强制中断poller_->poll()，从而让循环尽快推出
 * 
 * 比如在一个subloop(worker)中调用mainloop(IO)的quit时 需要唤醒mainloop(IO)的poller_->poll 让其执行完loop()函数
 *
 * ！！！ 注意： 正常情况下 mainloop负责请求连接 将回调写入subloop中 通过生产者消费者模型即可实现线程安全的队列
 * ！！！       但是muduo通过wakeup()机制 使用eventfd创建的wakeupFd_ notify 使得mainloop和subloop之间能够进行通信
 */
void EventLoop::quit(){
    quit_ = true;

    if(!isInLoopThread()){
        wakeup();
    }
}

// 用来唤醒loop所在线程 向eventfd(即wakeupFd_)写入一个8字节数据，触发eventfd生成EPOLLIN事件
// 事件触发后epoll_wait()立即返回打断阻塞，EventLoop检测到触发的Channel事件后，调用回调函数handleRead()进行处理
/** 底层触发机制：
 *  1.创建eventfd：eventfd()
 *  2.注册到epoll: epoll_ctl()
 *  3.触发事件: write()
 *  4.事件捕获: epoll_wait()
 *  5.事件回调: handleRead()
 *  6.恢复阻塞: epoll_wait()
 */
void EventLoop::wakeup(){
    // eventfd读写操作需要传递unit64_t大小的数据
    uint64_t one = 1;
    // write()是POSIX标准库函数，one值写入wakeupFd_，触发eventfd内部计数器
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

// 读取eventfd，将状态重置
void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){   //在当前EventLoop中执行回调
        cb();
    }
    else{
        queueInLoop(cb);    //在非当前EventLoop的线程中执行cb，需要唤醒Eventloop所在线程执行cb
    }
}

// 把cb放入队列 唤醒loop所在线程执行cb
void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);   //加锁保证线程安全
        pendingFunctors_.emplace_back(cb);  //将任务加入新队列
    }
    // || callingPendingFunctors_：当前线程在执行其他任务（即在 doPendingFunctors() 中执行任务）。
    // 如果在这个过程中有新的任务加入，queueInLoop() 仍然会触发 wakeup()，让 epoll_wait() 立即返回。这样下一个事件循环就会立刻执行新任务。
    if(!isInLoopThread() || callingPendingFunctors_){
        wakeup();
    }
}

// EventLooop 的方法 => Poller 的方法
void EventLoop::updateChannel(Channel *channel){
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors(){
    // 创建一个临时vector容器来存储执行的回调任务
    std::vector<Functor> functors;
    // 标志位，表示当前正在执行回调函数，防止其他线程并发修改pendingFunctors_
    callingPendingFunctors_ = true;

    {
        // 临界区的作用域为 {} 内部，出了作用域后自动释放锁，减少锁的持有时间，提升效率。
        std::unique_lock<std::mutex> lock(mutex_);
        // 交换两个vector容器，将当前loop需要执行的回调函数交换到局部变量functors中
        functors.swap(pendingFunctors_);
        /*
         * 避免死锁风险：
         * 如果 functor() 本身调用了 queueInLoop()，而 queueInLoop() 需要再次获取 mutex_ 锁，可能会导致死锁。
         * swap 提前移出了 pendingFunctors_ 的内容，避免 functor() 的递归操作影响原始容器。
         */
    }

    // 遍历执行回调函数, 在非临界区完成，保证执行过程中不阻塞其他线程调用queueInLoop()
    for(const Functor &functor: functors){
        functor();
    }

    // 标志回调执行完毕，允许其他线程继续将任务插入pendingFunctors_并触发回调
    callingPendingFunctors_ = false;
}
/*
问题：
    如果当前线程要去执行消费该线程EventLoop对应的任务队列里的回调函数，此时又有新的回调函数想加入到该队列中，
  那我们肯定没办法一边去遍历队列来执行消费任务队列中的回调函数，一边向任务队列加入新的任务，
  我们需要首先对队列加锁，执行完队列中的任务后再去解锁，然后再让其他回调函数能够加入到任务队列。但是这样肯定是不可行的，其效率是非常低的
解决方法：
    陈硕在doPendingFunctors()函数中，创建了一个栈空间的任务队列functors，
  然后对pendingFunctors_进行加锁仅仅只是为了将其队列中的回调函数换出到栈空间任务队列functors当中。
  换出完毕后遍历栈空间任务队列，并在栈空间任务队列中执行相应的回调任务，
  而pendingFunctors_则很快的解除互斥锁的持有，能够更加迅速的响应外部，让回调函数尽快加入到该任务队列中
*/