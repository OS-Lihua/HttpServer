#include "log.h"

namespace lihua
{
    static char *logLevelGet(const int level)
    { // 得到当前输入等级level的字符串
        if (level == DEBUG)
        {
            return (char *)"DEBUG";
        }
        else if (level == INFO)
        {
            return (char *)"INFO";
        }
        else if (level == NOTICE)
        {
            return (char *)"NOTICE";
        }
        else if (level == WARN)
        {
            return (char *)"WARN";
        }
        else if (level == ERROR)
        {
            return (char *)"ERROR";
        }
        else if (level == CRIT)
        {
            return (char *)"CRIT";
        }
        else if (level == ALERT)
        {
            return (char *)"ALERT";
        }
        else if (level == FATAL)
        {
            return (char *)"FATAL";
        }
        else
        {
            return (char *)"NOSET";
        }
    }

    // 日志输出函数
    void log(const int level, const char *fun, const int line, const char *fmt, ...) // 等级 函数 行号 ...
    {
#ifdef OPEN_LOG                                // 判断开关
        va_list arg;                           // 创建不定参数指针
        va_start(arg, fmt);                    // 绑定不定参数位置
        char buf[1024];                        // 创建缓存字符数组
        vsnprintf(buf, sizeof(buf), fmt, arg); // 将 ftm 及后序的 arg 赋值到 buf
        va_end(arg);                           // ！结束绑定
        if (level >= LOG_LEVEL)
        { // 判断当前日志等级，与程序日志等级状态对比
            printf("[%s]\t[%s %d]: %s \n", logLevelGet(level), fun, line, buf);
        }
#endif
    }
}