#pragma once

#include <string>

#include "ads_noncopyable.h"

// LOG_INFO(%s %d, arg1, arg2); 带参数的宏
// logmsgFormat：表示格式化字符串，比如 "The value is %d"
// ...：表示可变参数，即格式化字符串中可能包含的参数（如 int、string 等）

// ##__VA_ARGS__ 是一个GCC扩展，表示将可变参数即...，传递给snprintf()  
// ##用于在宏中拼接字符串。若参数为空，则##起到去掉前面逗号的作用  
#define LOG_INFO(logmsgFormat, ...)                         \
    do                                                      \
    {                                                       \
        Logger &logger = Logger::instance();                \
        logger.setLogLevel(INFO);                           \
        char buf[1024] = {0};                               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
    }while(0) 

#define LOG_ERROR(logmsgFormat, ...)                        \
    do                                                      \
    {                                                       \
        Logger &logger = Logger::instance();                \
        logger.setLogLevel(ERROR);                          \
        char buf[1024] = {0};                               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
    }while(0)

#define LOG_FATAL(logmsgFormat, ...)                        \
    do                                                      \
    {                                                       \
        Logger &logger = Logger::instance();                \
        logger.setLogLevel(FATAL);                          \
        char buf[1024] = {0};                               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
    }while(0)

// 在发布版本中通常不启用 LOG_DEBUG，以避免影响性能。
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                        \
    do                                                      \
    {                                                       \
        Logger &logger = Logger::instance();                \
        logger.setLogLevel(DEBUG);                          \
        char buf[1024] = {0};                               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
    }while(0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif


// 定义一个enum类的日志级别
// enum是一个枚举类型，定义一组具名的整数常量，每个枚举成员会被自动从0开始赋值为一个整数值
// C++11引入了enum class。enum class是一个强类型枚举，不会隐式转换为整数，更加安全。
// enum class的作用域为枚举类的作用域，不会污染其他作用域。需要使用Logger::LogLevel::INFO来访问
enum LogLevel{
    INFO,   // 普通信息
    ERROR,  // 错误信息
    FATAL,  // core dump信息.core dump（核心转储）是程序崩溃时，系统将程序的内存快照（包括堆栈、寄存器、全局变量等）保存到一个文件中的过程。
    DEBUG,  // 调试信息
};

class Logger : noncopyable{
public:
    // 获取日志唯一的实例对象 单例模式（保证一个类仅有一个实例，并提供一个访问它的全局访问点）
    static Logger &instance();  
    // 设置当前日志级别。在记录日志前，需要根据设置的日志级别来判断是否输出日志
    void setLogLevel(int level);
    // 将日志输出到控制台or文件。msg是格式化后的日志内容
    void log(std::string msg);
private:
    int logLevel_;
};