#include <strings.h>
#include <string.h>

#include "ads_InetAddress.h"

InetAddress::InetAddress(uint16_t port, std::string ip){
    // memset(void *s, int c, size_t n) —— 用于将内存块设置为指定的值。
    // &addr_ —— 指向要清空的内存（即 sockaddr_in 结构体）。0 —— 用 0 填充内存。sizeof(addr_) —— 填充的字节长度。
    // 目的是将 sockaddr_in 结构体的内存置为 0，防止其中存在未定义数据（初始化操作）
    ::memset(&addr_, 0, sizeof(addr_));
    // 设置地址族为 IPv4
    addr_.sin_family = AF_INET;
    // htons(uint16_t hostshort) —— 将 主机字节序（主机端存储方式）转换为 网络字节序（大端字节序）。在不同架构主机之间保证兼容性
    addr_.sin_port = htons(port);
    // ::inet_addr(const char *cp) —— 将点分十进制的 IP 地址（如 "192.168.1.1") 转换为 in_addr_t 类型的二进制格式（网络字节序）。
    // ip.c_str() —— 将 std::string 转换为 C 字符串（即 const char *）。
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());

    // 推荐使用以下
    /*
     * if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
        // 处理错误，例如抛出异常或记录日志
     * }
     */
}

std::string InetAddress::toIp() const{
    // 定义一个长度为 64 的缓冲区，初始化列表为0，保证内容干净
    char buf[64] = {0};
    // inet_ntop（network to presentation）将网络格式（即二进制）转换为人类可读的点分十进制格式。
    //inet_ntop() 的返回值是指向 buf 的指针（即转换后的 IP 字符串）。如果转换失败，返回 nullptr。把网络字符
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    // ::strlen(buf) 是一个 C 标准库函数，用于返回字符串 buf 的长度（不包括末尾的 '\0'）。end 保存当前字符串的长度。
    size_t end = strlen(buf);
    // ntohs(uint16_t netshort) —— 将 网络字节序（大端字节序）转换为 主机字节序（主机端存储方式）。
    uint16_t port = ntohs(addr_.sin_port);
    // sprintf() 的工作方式是：从 buf + end 的位置开始写入格式化字符串，在写入的最后自动加上 '\0' 结束符
    sprintf(buf + end, ":%u", port);
    return buf;
}

uint16_t InetAddress::toPort() const{
    return ntohs(addr_.sin_port);
}


// 测试代码块
#if 0
#include <iostream>
int main(){
    InetAddress addr(8080);
    std::cout << addr.toIpPort() << std:endl;
}
#endif