#pragma once

#include <arpa/inet.h>  //包含了 IP 地址转换相关的函数（如 inet_pton、inet_ntop 等
#include <netinet/in.h> //定义了与 sockaddr_in 结构体和网络地址相关的常量和数据结构（如 htons、ntohs 等）。这个头文件是 Linux/Unix 网络编程的基础
#include <string>

// 这个InetAddress类是muduo库中用于封装 IPv4 套接字地址（socket address） 的一个工具类，方便在网络编程中管理和操作地址信息
// InetAddress类的主要功能是将 IP 地址和端口号封装成一个对象，方便在网络编程中传递和使用
// InetAddress 简化了对 sockaddr_in 的操作，避免直接操作底层的 C 语言接口。
class InetAddress{
public:
    // explicit 关键字防止构造函数被隐式转换调用，防止发生隐式转换带来的歧义问题。
    // uint16_t port = 0：默认端口号为 0（表示由操作系统自动分配端口）。std::string ip = "127.0.0.1"：默认 IP 地址为 127.0.0.1（本地主机）
    explicit InetAddress(unit16_t port = 0, std::string ip = "127.0.0.1");
    // 允许通过一个 sockaddr_in 结构体直接初始化 InetAddress 对象。使用 成员初始化列表 直接对 addr_ 进行赋值。
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
        {
        }
    
    // 获取 IP 地址。 将 sockaddr_in 结构体中的 IP 地址转换为字符串格式（如 "192.168.1.1"）。const 关键字表示这个成员函数是只读的，不会修改类的成员变量。
    std::string toIp() const;
    // 获取 IP 地址和 端口号。将 sockaddr_in 结构体中的 IP 和端口号转换为 "192.168.1.1:8080" 格式。
    std::string toIpPort() const;
    // 返回 sockaddr_in 结构体中的端口号（通过 ntohs() 将网络字节序转换为主机字节序）。
    unit16_t toPort() const; 

    // 返回 sockaddr_in 结构体的指针。const 表示调用这个函数不会修改对象的状态。
    /* 两个const:
     * 1.返回类型中的 const：表示返回的指针指向的数据（即 sockaddr_in 结构体）是 常量的，调用者不能通过返回的指针来修改数据。
     * 2.函数声明末尾的 const：表示这个函数是一个 常量成员函数，即在函数内部不会修改对象的状态（不会修改任何成员变量）。
     */
    const sockaddr_in *getSockAddr() const {return &addr_;}

    // 通过一个 sockaddr_in 结构体来修改 InetAddress 对象的地址信息。
    void setSockAddr(const sockaddr_in &addr) {addr_ = addr;}

private:
    // sockaddr_in 是一个用于存储 IPv4 地址 的结构体
    // 定义在 <netinet/in.h> 中。addr_ 用于保存 IP 地址和端口号。
    sockaddr_in addr_;  
};