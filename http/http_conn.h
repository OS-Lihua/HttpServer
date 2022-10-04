#ifndef __HTTPCONNECTION_H__
#define __HTTPCONNECTION_H__

/*
    6. 查看定时器类 及main : signal操作
*/

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <utility>
#include "../lock/locker.h"
#include "../log/log.h"
#include "../cgi/connection_pool.h"

namespace lihua
{

    extern void addfd(int epollfd, int fd, bool one_shot);
    extern void removefd(int epollfd, int fd);
    extern int setnonblocking(int fd);
    class Http_conn
    {
    public:
        // 设置读取文件的名称m_real_file大小
        static const int FILENAME_LEN = 256;
        // 设置读缓冲区m_read_buf大小
        static const int READ_BUFFER_SIZE = 2048;
        // 设置写缓冲区m_write_buf大小
        static const int WRITE_BUFFER_SIZE = 2048;

        // 报文的请求方法，本项目只用到GET和POST
        enum METHOD
        {
            GET = 0,
            POST,
            HEAD,
            PUT,
            DELETE,
            TRACE,
            OPTIONS,
            CONNECT,
            PATH
        };

        // 主状态机的状态
        /*
            解析客户端请求时，主状态机的状态
            CHECK_STATE_REQUESTLINE:当前正在分析请求行
            CHECK_STATE_HEADER:当前正在分析头部字段
            CHECK_STATE_CONTENT:当前正在解析请求体
        */
        enum CHECK_STATE
        {
            CHECK_STATE_REQUESTLINE = 0, // 当前正在分析请求行
            CHECK_STATE_HEADER,          // 当前正在分析头部字段
            CHECK_STATE_CONTENT          // 当前正在解析请求体
        };
        // 报文解析的结果
        /*
           服务器处理HTTP请求的可能结果，报文解析的结果
           NO_REQUEST          :   请求不完整，需要继续读取客户数据
           GET_REQUEST         :   表示获得了一个完成的客户请求
           BAD_REQUEST         :   表示客户请求语法错误
           NO_RESOURCE         :   表示服务器没有资源
           FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
           FILE_REQUEST        :   文件请求,获取文件成功
           INTERNAL_ERROR      :   表示服务器内部错误,该结果在主状态机逻辑switch的default下,一般不会触发
           CLOSED_CONNECTION   :   表示客户端已经关闭连接了
       */
        enum HTTP_CODE
        {
            NO_REQUEST = 0,    // 请求不完整，需要继续读取客户数据
            GET_REQUEST,       // 表示获得了一个完成的客户请求
            BAD_REQUEST,       // 表示客户请求语法错误
            NO_RESOURCE,       // 表示服务器没有资源
            FORBIDDEN_REQUEST, // 表示客户对资源没有足够的访问权限
            FILE_REQUEST,      // 文件请求,获取文件成功
            INTERNAL_ERROR,    // 表示服务器内部错误
            CLOSED_CONNECTION  // 表示客户端已经关闭连接了
        };

        // 从状态机的状态
        // 从状态机的三种可能状态，即行的读取状态，分别表示
        // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
        enum LINE_STATUS
        {
            LINE_OK = 0, //完整读取一行
            LINE_BAD,    //报文语法有误
            LINE_OPEN    //读取的行不完整
        };

    public:
        explicit Http_conn() {}
        ~Http_conn() {}

    public:
        // 初始化套接字地址，函数内部会调用私有方法init
        void init(int sockfd, const sockaddr_in &addr); //*
        // 关闭http连接
        void close_conn(bool real_close = true); //*
        // 工作
        void process(); //*
        // 读取浏览器端发来的全部数据
        bool read_once(); //*
        bool write();     //*
        // 响应报文写入函数
        sockaddr_in *get_address() //*
        {
            return &m_address;
        }
        // 同步线程初始化数据库读取表
        void initmysql_result(Connection_pool *connPool);

    private:
        void init(); //*
        // 从m_read_buf读取，并处理请求报文
        HTTP_CODE process_read(); //*
        // 向m_write_buf写入响应报文数据
        bool process_write(HTTP_CODE ret); //*
        // 主状态机解析报文中的请求行数据
        HTTP_CODE parse_request_line(char *text); //*
        // 主状态机解析报文中的请求头数据
        HTTP_CODE parse_headers(char *text); //*
        // 主状态机解析报文中的请求内容
        HTTP_CODE parse_content(char *text); //*
        // 生成响应报文
        HTTP_CODE do_request(); //*

        // get_line用于将指针向后偏移，指向未处理的字符
        // 此时从状态机已提前将一行的末尾字符\r\n变为\0\0，所以text可以直接取出完整的行进行解析
        char *get_line() { return m_read_buf + m_start_line; };

        // 从状态机: 读取一行，分析是请求报文的哪一部分
        LINE_STATUS parse_line(); //*

        void unmap(); //*

        // 根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
        // 添加响应内容
        bool add_response(const char *format, ...); //*
        // 添加状态行
        bool add_status_line(int status, const char *title); //*
        // 添加消息报头，具体的添加文本长度、连接状态和空行
        bool add_headers(int content_length); //*
        // 添加文本类型，这里是html
        bool add_content_type(); //*
        // 添加连接状态，通知浏览器端是保持连接还是关闭//*
        bool add_linger(); //*
        // 添加Content-Length，表示响应报文的长度
        bool add_content_length(int content_length); //*
        // 添加空行
        bool add_blank_line(); //*
        // 添加消息文本 content
        bool add_content(const char *content); //*

    public:
        static int m_epollfd;    // epoll root
        static int m_user_count; // 用户连接数
        MYSQL *mysql;            // 数据库指针

    private:
        int m_sockfd;          //*
        sockaddr_in m_address; //*
        // 存储读取的请求报文数据
        char m_read_buf[READ_BUFFER_SIZE];
        // 缓冲区中m_read_buf中数据的最后一个字节的下一个位置    哨兵
        int m_read_idx;
        // m_read_buf解析的位置m_checked_idx    已经读取位置下一个位置 哨兵
        int m_checked_idx;
        // m_read_buf中已经解析的字符个数
        int m_start_line;

        //存储发出的响应报文数据
        char m_write_buf[WRITE_BUFFER_SIZE];
        //指示buffer中的长度
        int m_write_idx;

        //主状态机的状态
        CHECK_STATE m_check_state;
        //请求方法
        METHOD m_method;

        //以下为解析请求报文中对应的6个变量
        //存储读取文件的名称
        char m_real_file[FILENAME_LEN]; // 读取的文件名
        char *m_url;                    // url
        char *m_version;                // 版本号
        char *m_host;                   //
        int m_content_length;           // 请求体长度
        bool m_linger;                  // 是否是长连接

        char *m_file_address;    // 读取服务器上的文件地址
        struct stat m_file_stat; // 文件属性
        struct iovec m_iv[2];    // io向量机制iovec
        int m_iv_count;          //

        bool cgi;            // 是否启用的POST
        char *m_string;      // 存储请求头数据
        int bytes_to_send;   // 剩余发送字节数
        int bytes_have_send; // 已发送字节数
    };

};

#endif // ! __HTTPCONNECTION_H__