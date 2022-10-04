#ifndef __LST_TIMER__
#define __LST_TIMER__

#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include "../log/log.h"

namespace lihua
{

    class Util_timer;

    // 用户数据结构  作为callback的参数
    struct client_data
    {
        sockaddr_in address; // 客户端socket地址
        int sockfd;          // socket文件描述符
        Util_timer *timer;   // 定时器
    };

    // 定时器类
    class Util_timer
    {

    public:
        inline explicit Util_timer() : prev(NULL), next(NULL) {}

    public:
        time_t expire;                  // 任务超时时间，这里使用绝对时间
        void (*cb_func)(client_data *); // 任务回调函数，回调函数处理的客户数据，由定时器的执行者传递给回调函数
        client_data *user_data;         //连接资源
        Util_timer *prev;               // 指向前一个定时器
        Util_timer *next;               // 指向后一个定时器
    };

    // 定时器链表 类
    class Sort_timer_lst
    {
    private:
        Util_timer *head; // 头结点
        Util_timer *tail; // 尾结点
    public:
        inline explicit Sort_timer_lst() : head(NULL), tail(NULL){};
        // 链表被销毁时，删除其中所有的定时器
        inline ~Sort_timer_lst()
        {
            Util_timer *tmp = head;
            while (tmp)
            {
                head = tmp->next;
                delete tmp;
                tmp = head;
            }
        }

        // 将目标定时器timer添加到链表中
        void add_timer(Util_timer *timer);

        /* 当某个定时任务发生变化时，调整对应的定时器在链表中的位置。这个函数只考虑被调整的定时器的
        超时时间延长的情况，即该定时器需要往链表的尾部移动。*/
        void adjust_timer(Util_timer *timer);

        // 将目标定时器 timer 从链表中删除
        void del_timer(Util_timer *timer);

        /* SIGALARM 信号每次被触发就在其信号处理函数中执行一次 tick() 函数，以处理链表上到期任务。*/
        void tick();

    private:
        /* 一个重载的辅助函数，它被公有的 add_timer 函数和 adjust_timer 函数调用
        该函数表示将目标定时器 timer 添加到节点 lst_head 之后的部分链表中 */
        void add_timer(Util_timer *timer, Util_timer *lst_head);
    };

}

#endif // ! __LST_TIMER__