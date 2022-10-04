#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "../lock/locker.h"
#include "../cgi/connection_pool.h"
namespace lihua
{
    // 线程池类，模板参数T是任务类
    // 线程池基本逻辑
    // 1.创建/初始化线程池               构造
    // 2.把任务task 添加到任务队列中      append
    // 3.运行线程池                      worker/run
    // 4.任务运行的接口                   proccess
    template <typename T>
    class ThreadPool
    {
    public:
        /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
        ThreadPool(Connection_pool *connPool, int thread_number = 8, int max_requests = 10000);
        ~ThreadPool();
        bool append(T *request);

    private:
        //线程的数量
        int m_thread_number;

        //描述线程池的数组, 大小为 m_thread_number;
        pthread_t *m_threads; // 工作队列

        // 请求队列中最多允许的、等待处理的请求的数量
        int m_max_requests;

        // 请求队列
        std::list<T *> m_tasklist;

        // 保护请求队列的互斥锁
        Locker m_queuelocker;

        // 是否有任务需要处理
        Sem m_queuestat;

        // 是否结束线程
        bool m_stop;

        //数据库连接池
        Connection_pool *m_connPool;

    private:
        /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
        static void *worker(void *arg); // 线程的入口函数
        void run();                     // worker --> while(1){ run };
    };
};


#endif // ! __THREADPOOL_H__
