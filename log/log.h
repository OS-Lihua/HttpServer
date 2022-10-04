#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdarg.h>

#define OPEN_LOG 1     // 声明是否打开日志输出
#define LOG_LEVEL INFO // 声明当前程序的日志等级状态，只输出等级等于或高于该值的内容
#define LOG_SAVE 0     // 可补充日志保存功能

namespace lihua
{
    typedef enum
    { // 日志等级，越往下等级越高
      // 致命情况，系统不可用
        FATAL = 0,
        /// 高优先级情况，例如数据库系统崩溃
        ALERT,
        /// 严重错误，例如硬盘错误
        CRIT,
        /// 错误
        ERROR,
        /// 警告
        WARN,
        /// 正常但值得注意
        NOTICE,
        /// 一般信息
        INFO,
        /// 调试信息
        DEBUG

    } LOGLEVEL;

// 宏定义，隐藏形参
#define EMlog(level, fmt...) log(level, __FUNCTION__, __LINE__, fmt) // 函数 __FUNCTION__
                                                                     // 行号 __LINE__
    // 打印日志
    void log(const int level, const char *fun, const int line, const char *fmt, ...);

}

#endif
