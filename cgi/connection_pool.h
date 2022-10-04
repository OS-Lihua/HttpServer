#ifndef __CONNECTION_POOL__
#define __CONNECTION_POOL__

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <pthread.h>
#include "../lock/locker.h"

namespace lihua
{
    class Connection_pool
    {
    public:
        MYSQL *getConnection();              //获取数据库连接
        bool releaseConnection(MYSQL *conn); //释放连接
        int getFreeConn();                   //获取连接
        void destroyPool();                  //销毁所有连接

        void init(std::string Url, std::string User, std::string PassWord, std::string DataBaseName, int Port, unsigned int MaxConn);

        static Connection_pool *GetInstance();

    private:
        explicit Connection_pool();
        ~Connection_pool();

    private:
        unsigned int maxConn;  //最大连接数
        unsigned int curConn;  //当前已使用的连接数
        unsigned int freeConn; //当前空闲的连接数

    private:
        static Connection_pool *connPool; // 单例
        std::list<MYSQL *> connList;      // 连接池
        static Locker lock;               // 互斥量
        Sem sem;                          // 信号量
    private:
        std::string url;          //主机地址
        std::string port;         //数据库端口号
        std::string user;         //登陆数据库用户名
        std::string passWord;     //登陆数据库密码
        std::string databaseName; //使用数据库名
    };
   

    // 利用自动析构实现RAII
    class ConnectionRAII
    {
    public:
        explicit ConnectionRAII(MYSQL **sql, Connection_pool *connPool);
        ~ConnectionRAII();

    private:
        MYSQL *conRAII;
        Connection_pool *poolRAII;
    };

}

#endif // ! __CONNECTION_POOL__
