#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<signal.h>
#include<sys/wait.h>
#include<sys/stat.h>

#include"15_1_processpool.h"

class cgi_conn{ //  用于处理客户端请求的类
private:
    //  用于读取数据的缓冲区大小
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;

public:
    cgi_conn(){}
    ~cgi_conn(){}   
    void init(int epollfd,int sockfd,const sockaddr_in &client_addr){
        m_epollfd = epollfd;//  初始化epollfd
        m_sockfd = sockfd;//  初始化sockfd
        m_address = client_addr;//  初始化客户端地址
        memset(m_buf,'\0',BUFFER_SIZE); //  初始化缓冲区
        m_read_idx = 0;
    }   

    void process(){
        int idx = 0;;
        int ret = -1;
        while(1){
            int idx = m_read_idx;
            ret = recv(m_sockfd,m_buf+idx,BUFFER_SIZE-1-idx,0);// 读取客户端数据
            if(ret<0){
                if(errno !=EAGAIN ){
                    removefd(m_epollfd,m_sockfd);
                }
                break;
            }
            else if(ret == 0){
                removefd(m_epollfd,m_sockfd);
                break;
            }
            else{
                m_read_idx += ret;
                printf("user content is: %s\n",m_buf);
                // 如果遇到字符"\r\n"，则开始处理客户请求
                for(;idx<m_read_idx;++idx){
                    if(idx>=1 && m_buf[idx-1]=='\r' && m_buf[idx]=='\n'){
                        break;
                    }
                }
                // 如果没有遇到字符"\r\n"，则需要读取更多客户数据
                if( idx == m_read_idx){
                    continue;
                }
                m_buf[ idx-1 ]  = '\0';

                char *file_name = m_buf;
                if(access(file_name,F_OK) == -1){
                    removefd(m_epollfd,m_sockfd);
                    break;
                }
                ret = fork();
                if(ret == -1){
                    removefd(m_epollfd,m_sockfd);
                    break;
                }
                else if(ret > 0){
                    removefd(m_epollfd,m_sockfd);
                    break;
                }
                else{
                    close(STDOUT_FILENO);
                    dup(m_sockfd);//  将标准输出重定向到m_sockfd
                    execl(m_buf,m_buf,0);//  执行CGI程序
                    exit(0);
                }
            }
        }
    }

};

int cgi_conn::m_epollfd = -1;

int main(int argc,char *argv[]){
    if(argc <= 2){
        printf("%d\n",basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd >=0);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port = port;

    ret = bind(listenfd,(struct sockaddr *) &address,sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd,5);
    assert(ret != -1);

    processpool<cgi_conn>* pool = processpool<cgi_conn>::create(listenfd);
    if(pool){
        pool->run();
        delete pool;
    }
    close(listenfd);//main创建就由main销毁，而不是在进程池中销毁
    return 0;
}