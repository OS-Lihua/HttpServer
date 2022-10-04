#ifndef __LOCKER_H__
#define __LOCKER_H__

#include <pthread.h>
#include <exception>
#include <semaphore.h>

namespace lihua
{
    // 线程同步机制封装类
    // 将重复使用的代码封装为函数，减少代码的重复，使其更简洁

    // 互斥量
    class Locker
    {
    public:
        Locker()
        {
            if (pthread_mutex_init(&m_mutex, NULL) != 0)
            {
                throw std::exception{};
            }
        }

        ~Locker()
        {
            pthread_mutex_destroy(&m_mutex); // 释放
        }

    private:
        pthread_mutex_t m_mutex; //互斥量类型

    public:
        bool lock()
        {
            return pthread_mutex_lock(&m_mutex) == 0; // 上锁
        }

        bool unlock()
        {
            return pthread_mutex_unlock(&m_mutex) == 0; // 解锁
        }

        pthread_mutex_t *get()
        {
            return &m_mutex;
        }
    };

    // 条件变量类
    class Cond
    {
    public:
        Cond()
        {
            if (pthread_cond_init(&m_cond, NULL) != 0)
            { // 初始化
                throw std::exception();
            }
        }

        ~Cond()
        {
            pthread_cond_destroy(&m_cond); // 释放
        }

    private:
        pthread_cond_t m_cond; // 条件变量类型
    public:
        //阻塞等待signal
        bool wait(pthread_mutex_t *mutex)
        {
            return pthread_cond_wait(&m_cond, mutex) == 0; // 等待
        }

        // 在一定时间等待
        bool timedwait(pthread_mutex_t *mutex, struct timespec t)
        {
            return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
        }

        // 唤醒一个/多个线程
        bool signal()
        {
            return pthread_cond_signal(&m_cond) == 0;
        }
        // 唤醒所有线程
        bool broadcast()
        {
            return pthread_cond_broadcast(&m_cond) == 0;
        }
    };

    // 信号量
    class Sem
    {
    public:
        //构造函数
        Sem()
        {
            // 信号量值初始化为0 （消费者信号量）
            if (sem_init(&m_sem, 0, 0) != 0)
            {
                throw std::exception();
            }
        }
        // 传参创建
        Sem(int num)
        {
            //信号量初始化
            if (sem_init(&m_sem, 0, num) != 0)
            {
                throw std::exception();
            }
        }
        //析构函数
        ~Sem()
        {
            //信号量销毁
            sem_destroy(&m_sem);
        }

    private:
        sem_t m_sem;

    public:
        // 等待（减少）信号量
        bool wait()
        {
            return sem_wait(&m_sem) == 0;
        }

        // 增加信号量
        bool post()
        {
            return sem_post(&m_sem) == 0;
        }
    };
};

#endif // ! __LOCKER_H__
