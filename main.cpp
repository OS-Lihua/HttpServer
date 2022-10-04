#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./lock/locker.h"
#include "./threadpool/threadpool.cpp"
#include "./timer/lst_timer.h"
#include "./http/http_conn.h"
#include "./log/log.h"
#include "./cgi/connection_pool.h"

using namespace lihua;

#define MAX_FD 65536           //最大文件描述符
#define MAX_EVENT_NUMBER 10000 //最大事件数
#define TIMESLOT 5             //最小超时单位

#define PORT 8888 // 默认端口

//#define listenfdET //边缘触发非阻塞
#define listenfdLT //水平触发阻塞

//设置定时器相关参数
static int pipefd[2];
static Sort_timer_lst timer_lst;
static int epollfd = 0;

//信号处理函数
void sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;

    //将信号值从管道写端写入，传输字符类型，而非整型
    send(pipefd[1], (char *)&msg, 1, 0);

    //将原来的errno赋值为当前的errno
    errno = save_errno;
}

//设置信号函数
void addsig(int sig, void(handler)(int), bool restart = true)
{
    //创建sigaction结构体变量
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    //信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART; //使被信号打断的系统调用自动重新发起
    //将所有信号添加到信号集中
    sigfillset(&sa.sa_mask);
    //执行sigaction函数
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void timer_handler()
{
    timer_lst.tick();
    alarm(TIMESLOT);
}

//定时器回调函数，删除非活动连接在socket上的注册事件，并关闭
void cb_func(client_data *user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    Http_conn::m_user_count--;
    EMlog(INFO, "close fd %d", user_data->sockfd);
}

// 向连接的fd 发送错误号
void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}
// 初始化连接池
Connection_pool *init_conn(int num)
{
    //创建数据库连接池
    Connection_pool *connPool = Connection_pool::GetInstance();
    connPool->init("localhost", "root", "root", "lhdb", 3306, num);
    return connPool;
}
// 初始化线程池
ThreadPool<Http_conn> *init_thread(Connection_pool *connPool)
{
    //创建线程池
    ThreadPool<Http_conn> *pool = NULL;
    try
    {
        pool = new ThreadPool<Http_conn>(connPool);
    }
    catch (...)
    {
        return nullptr;
    }
    return pool;
}

// 初始化本地socket  create-->bind-->listen
int init_socket(int port)
{
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);
    return listenfd;
}

// 初始化epfd
void init_epfd()
{
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    Http_conn::m_epollfd = epollfd;
}
// 初始化管道
void init_pair()
{
    //创建管道
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    //设置管道读端为ET非阻塞
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);
    //传递给主循环的信号值，这里只关注SIGALRM和SIGTERM
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
}

// 初始化client_data数据
// 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
void add_timer_to_list(client_data *users_timer, int connfd, struct sockaddr_in *client_address)
{
    users_timer[connfd].address = *client_address;
    users_timer[connfd].sockfd = connfd;
    Util_timer *timer = new Util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    timer_lst.add_timer(timer);
}
// 主函数
int main(int argc, char *argv[])
{
    int port = PORT;

    if (argc >= 2)
    {
        port = atoi(argv[1]);
    }

    addsig(SIGPIPE, SIG_IGN);

    // 初始化连接池
    Connection_pool *connPool = init_conn(8);

    // 初始化线程池
    ThreadPool<Http_conn> *pool = init_thread(connPool);

    Http_conn *users = new Http_conn[MAX_FD];
    assert(users);

    //初始化数据库读取表
    users->initmysql_result(connPool);

    // 初始化 socket
    int listenfd = init_socket(port);

    // 初始化epoll 树
    init_epfd();

    addfd(epollfd, listenfd, false);

    // 初始化管道
    init_pair();

    bool stop_server = false;
    client_data *users_timer = new client_data[MAX_FD];

    bool timeout = false;
    // 设置定时器
    alarm(TIMESLOT);
    int ret = 0;

    //创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            EMlog(ERROR, "%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
#ifdef listenfdLT
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0)
                {
                    EMlog(ERROR, "%s:errno is:%d", "accept error", errno);
                    continue;
                }
                if (Http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    EMlog(ERROR, "%s", "Internal server busy");
                    continue;
                }
                users[connfd].init(connfd, client_address);

                // 创建初始化定时器并加入链表中
                add_timer_to_list(users_timer, connfd, &client_address);

#endif
#ifdef listenfdET
                //  差别就在分多次和一次性读完
                while (1)
                {
                    int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                    if (connfd < 0)
                    {
                        EMlog(ERROR, "%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    if (Http_conn::m_user_count >= MAX_FD)
                    {
                        show_error(connfd, "Internal server busy");
                        EMlog(ERROR, "%s", "Internal server busy");
                        break;
                    }
                    users[connfd].init(connfd, client_address);

                    // 创建初始化定时器并加入链表中
                    add_timer_to_list(users_timer, connfd, &client_address);
                }
                continue;
#endif
            }

            // 如果是出错了 或者  EPOLLRDHUP 对端断开
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                Util_timer *timer = users_timer[sockfd].timer;
                timer->cb_func(&users_timer[sockfd]);

                if (timer)
                {
                    timer_lst.del_timer(timer);
                }
            }

            //处理信号
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) // 如果接收到信号
            {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0); // 读出来是信号
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; ++i)
                    {
                        switch (signals[i])
                        {
                        case SIGALRM: // 如果是定时器
                        {
                            timeout = true;
                            break;
                        }
                        case SIGTERM: //如果是KILL
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
            }

            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                Util_timer *timer = users_timer[sockfd].timer;
                if (users[sockfd].read_once())
                {
                    EMlog(INFO, "deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

                    //若监测到读事件，将该事件放入请求队列
                    pool->append(users + sockfd);

                    //若有数据传输，则将定时器往后延迟3个单位
                    //并对新的定时器在链表上的位置进行调整
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        EMlog(INFO, "%s", "adjust timer once");

                        timer_lst.adjust_timer(timer);
                    }
                }
                else
                {
                    // 删除
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                Util_timer *timer = users_timer[sockfd].timer;
                if (users[sockfd].write())
                {
                    EMlog(INFO, "send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

                    //若有数据传输，则将定时器往后延迟3个单位
                    //并对新的定时器在链表上的位置进行调整
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        EMlog(INFO, "%s", "adjust timer once");

                        timer_lst.adjust_timer(timer);
                    }
                }
                else
                {
                    // 删除
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
            }
        }
        if (timeout)
        {
            timer_handler();
            timeout = false;
        }
    }
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete pool;
    return 0;
}
