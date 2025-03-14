#include <iostream>

#include "ads_Logger.h"
#include "ads_Timestamp.g"

Logger &Logger::instance(){
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level){
    logLevel_ = level;
}

// 写日志[级别] time : msg
void Logger::log(std::string msg){
    std::string pre = "";
    switch(logLevel_){
        case INFO:
            pre = "[INFO]";
            break;
        case ERROR:
            pre = "[ERROR]";
            break;
        case FATAL:
            pre = "[FATAL]";
            break;
        case DEBUG:
            pre = "[DEBUG]";
            break;
        default:
            break;
    }

    //输出至控制台，时间和msg
    std::cout << Timestamp::now().toString() << " : " << msg << std::endl; 
}