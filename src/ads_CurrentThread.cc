#include "ads_CurrentThread.h"

namespace CurrentThread{
    // 将t_cachedTid初始化为0，表示当前线程的 tid 尚未缓存。
    __thread int t_cachedTid = 0;

    void cacheTid(){
        if(t_cachedTid == 0){
            // syscall() 是一个底层系统调用接口，直接与 Linux 内核通信。
            // SYS_gettid 是系统调用号，表示获取当前线程的 ID。
            // syscall(SYS_gettid) 会返回一个整数，表示当前线程的 ID。
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
            // 将tid缓存到t_cachedTid中，系统调用获取 tid 的代价高，缓存后可以在后续调用中直接返回，避免重复系统调用，提升性能。
        }
    }
}