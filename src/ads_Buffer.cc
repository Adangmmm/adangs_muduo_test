#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include "ads_Buffer.h"

/* 从fd上读取数据 Poller工作在LT模式
 * Buffer的大小是预设的，但从fd读数据时不知道tcp数据最终大小
 * 
 * 故从socket读到缓冲区的方法是使用readv先读到buffer_，
 * 如果buffer_空间不够，则先读到栈上的65536个字节大小的空间，然后再append进buffer_
 * readv() 支持分散读取（scatter input）：
 *      减少系统调用次数：一次 readv() 可填充多个缓冲区，避免 read() 需要多次调用。
 *      提高性能：减少 syscall 的开销，在网络编程中提高吞吐量。
 */
// saveErrno是指向errno的指针，用于存储deadv()失败时的错误码
ssize_t Buffer::readFd(int fd, int *saveErrno){
    // 栈上的额外空间，在buffer_扩容时暂存数据，65536/1024 = 64kB
    char extrabuf[65536] = {0};

    /**
     * iovec 结构体用于 分散/聚集 I/O（Scatter/Gather I/O），可让 readv() 一次读取到多个缓冲区：
        struct iovec {
        void *iov_base; // 指向数据缓冲区的指针
        size_t iov_len; // 缓冲区大小
        };
     */
    // 分配两个连续的缓冲区
    struct iovec vec[2];
    cosnt size_t writeable = writeableBytes();

    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writeable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.
    // 如果 Buffer 空间足够大（大于 extrabuf），只使用 vec[0]（iovcnt = 1）。
    // 如果 Buffer 空间不够，就同时使用 vec[0] 和 vec[1]（iovcnt = 2）。
    const int iovcnt = (writeable < sizeof(extrabuf) ? 2 : 1);
    // readv()从fd读入数据，依次存入vec[0]和vec[1]指定的缓冲区
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if(n < 0){
        // 读数据错误
        *saveErrno = errno;
    }
    else if(n <= writeable){
        // buffer_够大，直接放进buffer_更新writerIndex_
        writerIndex_ += n;
    }
    else{
        // buffer_不够大，先填满buffer_，更新writerIndex_
        // 再把多的数据暂存栈上的extrabuf，待Buffer扩容后，从extrabuf上append进Buffer
        writerIndex_ = buffer.size();
        append(extrabuf, n - sizeof(extrabuf));
    }
    return n;   //返回读取数据的字节数
    
}

ssieze_t Buffer::writeFd(int fd, int *saveErrno){
    ssize_t n = ::write(fd, peek(), readableBytes())
    if(n < 0){
        *saveErrno = errno;
    }
    return n;
}