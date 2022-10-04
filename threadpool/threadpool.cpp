#include "threadpool.h"

namespace lihua
{
    template <typename T>
    ThreadPool<T>::ThreadPool(Connection_pool *connPool, int thread_number, int max_requests)
        : m_thread_number{thread_number}, m_max_requests{max_requests}, m_stop{false}, m_threads{NULL}, m_connPool{connPool}
    {

        if ((thread_number <= 0) || (max_requests <= 0))
        {
            throw std::exception();
        }

        m_threads = new pthread_t[m_thread_number];
        if (!m_threads)
        {
            throw std::exception();
        }

        // 创建thread_number 个线程，并将他们设置为脱离线程。
        for (int i = 0; i < thread_number; ++i)
        {
            printf("create the %dth thread\n", i);
            if (pthread_create(m_threads + i, NULL, worker, this) != 0)
            {
                delete[] m_threads;
                throw std::exception();
            }

            if (pthread_detach(m_threads[i]))
            {
                delete[] m_threads;
                throw std::exception();
            }
        }
    }

    template <typename T>
    ThreadPool<T>::~ThreadPool()
    {
        delete[] m_threads;
        m_stop = true;
    }

    // 将任务加入请求队列
    template <typename T>
    bool ThreadPool<T>::append(T *request)
    {
        // 操作工作队列时一定要加锁，因为它被所有线程共享。
        m_queuelocker.lock();
        if (m_tasklist.size() > m_max_requests)
        {
            m_queuelocker.unlock();
            return false;
        }
        m_tasklist.push_back(request);
        m_queuelocker.unlock();
        m_queuestat.post();
        return true;
    }

    template <typename T> //线程池工作
    void *ThreadPool<T>::worker(void *arg)
    {
        ThreadPool *pool = (ThreadPool *)arg;
        pool->run();
        return nullptr;
    }

    template <typename T>
    void ThreadPool<T>::run()
    {

        while (!m_stop)
        {
            m_queuestat.wait();
            m_queuelocker.lock();
            if (m_tasklist.empty())
            {
                m_queuelocker.unlock();
                continue;
            }
            T *request = m_tasklist.front();
            m_tasklist.pop_front();
            m_queuelocker.unlock();

            if (!request)
            {
                continue;
            }

            ConnectionRAII mysqlcon(&request->mysql, m_connPool); //调用连接池,用完就放回去
            request->process();                                   // 任务工作     这是一个接口
        }
    }
}