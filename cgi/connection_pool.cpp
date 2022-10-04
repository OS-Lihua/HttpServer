#include "connection_pool.h"

namespace lihua
{
    Connection_pool *Connection_pool::connPool = nullptr;
    Locker Connection_pool::lock = Locker{};

    Connection_pool::Connection_pool()
    {
        curConn = 0;
        freeConn = 0;
    }

    Connection_pool *Connection_pool::GetInstance()
    {
        lock.lock();
        if (connPool == nullptr)
        {
            connPool = new Connection_pool();
        }
        lock.unlock();
        return connPool;
    }
    void Connection_pool::init(std::string Url, std::string User, std::string PassWord, std::string DataBaseName, int Port, unsigned int MaxConn)
    {
        url = Url;
        port = Port;
        user = User;
        passWord = PassWord;
        databaseName = DataBaseName;

        lock.lock();
        for (int i = 0; i < MaxConn; i++)
        {
            MYSQL *sql = NULL;
            sql = mysql_init(sql); // 创建数据库
            if (sql == NULL)
            {
                std::cout << "Error:" << mysql_error(sql);
                exit(1);
            }

            //建立连接
            sql = mysql_real_connect(sql, Url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);
            if (sql == NULL)
            {
                std::cout << "Error:" << mysql_error(sql);
                exit(1);
            }

            connList.push_back(sql);
            ++freeConn;
        }

        sem = Sem(freeConn); //初始化连接数 Sem(MaxConn)

        this->maxConn = freeConn;

        lock.unlock();
    }

    //当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
    MYSQL *Connection_pool::getConnection()
    {
        MYSQL *con = NULL;

        if (0 == connList.size())
            return NULL;

        sem.wait(); // 如果信号量为0,表示连接池全部已经被占用，等待释放

        lock.lock();

        con = connList.front();
        connList.pop_front();

        --freeConn;
        ++curConn;

        lock.unlock();
        return con;
    }

    //释放当前使用的连接
    bool Connection_pool::releaseConnection(MYSQL *con)
    {
        if (NULL == con)
            return false;

        lock.lock();

        connList.push_back(con);
        ++freeConn;
        --curConn;

        lock.unlock();

        sem.post();
        return true;
    }

    //销毁数据库连接池
    void Connection_pool::destroyPool()
    {

        lock.lock();
        if (connList.size() > 0)
        {
            std::list<MYSQL *>::iterator it;
            for (it = connList.begin(); it != connList.end(); ++it)
            {
                MYSQL *sql = *it;
                mysql_close(sql); // 销毁sql
            }
            curConn = 0;
            freeConn = 0;
            connList.clear();
        }

        lock.unlock();
    }

    //当前空闲的连接数
    int Connection_pool::getFreeConn()
    {
        return freeConn;
    }

    Connection_pool::~Connection_pool()
    {
        destroyPool();
    }

    ConnectionRAII::ConnectionRAII(MYSQL **sql, Connection_pool *connPool)
    {
        *sql = connPool->getConnection(); // 从连接池中获取连接

        conRAII = *sql;
        poolRAII = connPool;
    }

    ConnectionRAII::~ConnectionRAII()
    {
        poolRAII->releaseConnection(conRAII);
    }
}