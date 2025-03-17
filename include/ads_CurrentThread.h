#pragma once

// 提供对 POSIX 操作系统 API 的访问，如 getpid()、read() 等。
#include <unistd.h>
// 定义了 syscall() 函数及其系统调用号，允许直接发起内核系统调用。
#include <sys/syscall.h>

// 定义命名空间，封装了与当前线程 ID 相关的功能，防止命名冲突。
namespace CurrrentThred{
    // __thread 是 GCC/Clang 提供的线程局部存储（TLS）修饰符。
    // 表示t_cachedTid是线程局部的，每个线程都有自巿的t_cachedTid变量。每个线程对t_cachedTid的读写是相互独立的
    // 每个线程的 tid 在第一次获取后缓存在 t_cachedTid 中，后续读取更快 
    extern _thread int t_cachedTid;

    // 用于获取当前线程的 tid，并将其缓存在 t_cachedTid 中，减少系统调用的频率。
    // tid 的获取通过 syscall(SYS_gettid) 完成，系统调用本身开销较高，因此需要缓存。
    void cacheTid();

    // 获取当前线程的tid
    // inline表示将函数在调用处直接展开，减少函数调用的开销。inline 仅是一个提示，具体是否内联由编译器决定。
    inline int tid(){
        // __built_expect() 是GCC/Clang提供的分支预测优化指令。__built_expect(x,0)提示编译器x的值更可能为0
        // t_cachedTid == 0 表示当前线程的 tid 尚未缓存，需要调用 cacheTid() 获取。
        if(_built_expect(t_cachedTid == 0, 0)){
            cacheTid();
        }
    }

    return t_cachedTid;
}
