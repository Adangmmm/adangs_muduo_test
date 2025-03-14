#include <time.h>

#include "ads_Timestamp.h"

Timestamp::Timestamp() : microSecondsSinceEpoch_(0)
{
}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpcch_(microSecondsSinceEpoch)
{
}

Timestamp Timestamp::now(){
    // time() 函数返回自Unix纪元（1970年1月1日）以来的秒数
    // 通过 time() 函数获取当前时间，然后转换为 Timestamp 对象返回
    return Timestamp(time(NULL));
}

std::string Timestamp::toString() const{
    char buf[128] = {0};
    // tm 是一个结构体，用于保存时间信息。localtime() 函数将 time_t 类型的时间转换为 tm 结构体类型的时间。
    // localtime() 函数返回的是一个指向静态分配的 tm 结构体的指针，因此不需要手动释放内存。
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    // snprintf() 是一个安全的字符串格式化函数，它将格式化后的字符串写入 buf 中，最大写入 128 个字符。
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,   //tm_mon 从 0 开始
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}

// 测试代码块
#if 0
#include <iostram>
int main(){
    std::cout << Timestamp::now().toString() << std::endl;
}
#endif