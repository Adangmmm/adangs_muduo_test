#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

class Buffer{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {
    }

    // 获取可读/可写区域大小
    size_t readableBytes() const {return writerIndex_ - readerIndex_;}
    size_t writeableBytes() const { return buffer_.size() - writerIndex_;}
    size_t prependableBytes() const {return readerIndex_;}

    // 返回放弃可读数据首地址
    // 第一个const表示返回的指针所指向的内容不可修改，char *指针指向char类型数据
    // 第二个const修饰peek()为常量成员函数，表示不会修改类的成员变量
    const char *peek() const {return begin() + readerIndex_;}

    // 消费len长度的数据
    void retrieve(size_t len){
        if(len < readableBytes()){
            readerIndex_ += len;
        }
        else{
            retrieveAll();
        }
    }
    //清空缓冲区
    void retrieveAll(){
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 将Buffer缓冲区中所有可读的数据转换为std::string并返回，同时更新读索引（相当于“取走”数据）
    std::string retrieveAllAsString() {return retrieveAsString(readableBytes());}
    // 从Buffer缓冲区拷贝len字节的数据，并返回为std::string
    std::string retrieveAsString(size_t len){
        // peek() 返回当前可读数据的起始地址 (const char *)。
        // std::string 有一个 接受 char* 和 size_t 作为参数的构造函数，它会从 peek() 处 拷贝 len 个字节 生成 result。
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    // 保证可写缓冲区够用
    void ensureWriteable(size_t len){
        if(writeableBytes() < len){
            makeSpace(len);
        }
    }

    // 把[data, data+len]内存上的数据添加到writeable缓冲区中
    void append(const char *data, size_t len){
        ensureWriteable(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }
    char *beginWrite() {return begin() + writerIndex_;}
    const char *beginWrite() const {return begin() + writerIndex_;}

    // 从fd上读数据
    ssize_t readFd(int fd, int *saveErrno);
    // 从fd上发数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // buffer_.begin() 返回 std::vector<char>::iterator（迭代器）。
    // *buffer_.begin() 解引用迭代器，获得第一个元素的引用（char&）。
    // &*buffer_.begin() 取得这个元素的地址，得到 指向 char 的指针。
    // 普通版，允许修改数据
    char *begin() {return &*buffer_.begin();}
    // 常量版，用于 cosnt Buffer，只允许读取，防止修改数据
    const char *begin() const {return &*buffer_.begin();}

    void makeSpace(size_t len){
        // 要是可写区域＋可读索引之前的区域  小于  需要的空间 + 预留空间，就扩容
        if(writeableBytes() + prependableBytes() < len + kCheapPrepend){
            buffer_.resize(writerIndex_ + len);
        }
        else{
            size_t readable = readableBytes();
            // 把可读区域的数据搬到一开始的kCheapPrepend上
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

};