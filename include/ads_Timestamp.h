#pragma once

#include <iostream>
#include <string>

class Timestamp{
public:
    Timestamp();
    // explicit 关键字防止构造函数被隐式转换调用，防止发生隐式转换带来的歧义问题。
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
private:
    // 它保存时间戳的值，表示从纪元以来的微秒数。
    // 使用 int64_t 类型是因为时间戳的值可能非常大，并且微秒级别的时间计算需要足够大的存储范围。
    int64_t microSecondsSinceEpoch_;
};