#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include<unistd.h> //   read write close
#include<signal.h> // 信号处理
#include<sys/types.h>   //  socket bind listen accept
#include<sys/epoll.h>
#include<fcntl.h> 
#include<sys/socket.h>
#include<netinet/in.h>// sockaddr_in
#include<arpa/inet.h> /// inet_pton
#include<assert.h>
#include<sys/stat.h>    // stat
#include<string.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>// mmap
#include<stdarg.h>// va_list va_start va_end
#include<errno.h>
#include "14_2_locker.h"

class http_conn{
public:
    static const int FILENAME_LEN = 200; // 文件名的最大长度
    static const int READ_BUFFER_SIZE = 2048; // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024; // 写缓冲区的大小
    enum METHOD{GET = 0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATCH}; // HTTP请求方法
    enum CHECK_STATE{CHECK_STATE_REQUESTLINE = 0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT}; // 解析客户请求时的主状态机状态
    enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION}; // 服务器处理HTTP请求的可能结果
    enum LINE_STATUS{LINE_OK = 0,LINE_BAD,LINE_OPEN}; // 行的读取状态
public:
    http_conn(){}
    ~http_conn(){}
public:
    void init(int sockfd,const sockaddr_in &addr);// 初始化新接受的连接
    void close_conn(bool real_close = true);// 关闭连接
    void process();// 处理客户请求
    bool read();// 非阻塞读操作
    bool write();// 非阻塞写操作
    
private:
    void init();
    HTTP_CODE process_read();// 解析HTTP请求
    bool process_write(HTTP_CODE ret);// 填充HTTP应答
    // 下面这一组函数被process_read调用以分析HTTP请求
    HTTP_CODE parse_request_line(char *text);// 解析请求行
    HTTP_CODE parse_headers(char *text);// 解析请求头
    HTTP_CODE parse_content(char *text);// 解析消息体
    HTTP_CODE do_request();// 处理请求
    char *get_line(){ return m_read_buf + m_start_line; }// 获取一行
    LINE_STATUS parse_line();// 分析一行

    // 下面这一组函数被process_write调用以填充HTTP应答
    void unmap();// 释放资源
    bool add_response(const char *format,...);// 向m_write_buf中写入响应
    bool add_content(const char *content);// 向m_write_buf中写入内容
    bool add_status_line(int status,const char *title);// 向m_write_buf中写入状态行
    bool add_headers(int content_length);// 向m_write_buf中写入响应头
    bool add_content_length(int content_length);// 向m_write_buf中写入Content-Length
    bool add_linger();// 向m_write_buf中写入Connection
    bool add_blank_line();// 向m_write_buf中写入空行
public:
    // 所有socket上的事件都被注册到同一个epoll内核事件表中，所以epoll文件描述符设置为静态的
    static int m_epollfd;
    static int m_user_count;// 统计用户数量

private:
    int m_sockfd;//  该HTTP连接的socket和对方的socket地址
    sockaddr_in m_address;//  对方的socket地址
    char m_read_buf[READ_BUFFER_SIZE];//  读缓冲区
    int m_read_idx;//  标识读缓冲区中已经读入的客户数据的最后一个字节的下一个位置
    int m_checked_idx;//  当前正在分析的字符在读缓冲区中的位置
    int m_start_line;//  当前正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE];//  写缓冲区
    int m_write_idx;//  写缓冲区中待发送的字节数
    CHECK_STATE m_check_state;//  主状态机当前所处的状态
    METHOD m_method;//  请求方法
    char m_real_file[FILENAME_LEN];//  客户请求的目标文件的完整路径，其内容等于doc_root+m_url,doc_root是网站根目录
    char *m_url;//  客户请求的目标文件的文件名
    char *m_version;//  HTTP协议版本号，我们仅支持HTTP/1.1
    char *m_host;//  主机名
    int m_content_length;//  HTTP请求的消息体的长度
    bool m_linger;//  HTTP请求是否要求保持连接
    char *m_file_address;//  客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;//  目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];//  采用writev来执行写操作，定义两个成员，其中一个指向m_write_buf,另一个指向m_file_address
    int m_iv_count;//  表示被写内存块的数量
};


#endif