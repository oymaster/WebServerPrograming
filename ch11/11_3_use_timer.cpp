#include <stdio.h>       // 包含标准输入输出库，用于基本的输入输出操作，例如 printf()。
#include <string.h>      // 包含字符串操作函数，例如 bzero() 和 strlen()。
#include <sys/socket.h>  // 包含套接字相关函数和定义，例如 socket(), bind(), listen(), accept() 等。
#include <netinet/in.h>  // 包含 Internet 地址族的结构和常量定义，例如 sockaddr_in 结构、htons() 和 AF_INET。
#include <arpa/inet.h>   // 包含函数用于将 IP 地址转换为标准格式，例如 inet_pton() 和 inet_ntop()。
#include <signal.h>      // 包含信号处理的函数和定义，例如 signal() 和 SIGTERM 等。
#include <unistd.h>      // 包含 POSIX 操作系统 API 函数，例如 close() 和 sleep()，以及一些系统调用。
#include <stdlib.h>      // 包含常用的标准库函数，例如 atoi()（将字符串转换为整数）。
#include <assert.h>      // 包含断言函数 assert()，用于在调试时进行条件检查。
#include <errno.h>       // 包含错误号定义和解释，errno 用于存储系统调用的错误码。
#include <stdbool.h>     // 包含布尔类型定义，例如 bool, true, false（从 C99 标准开始支持）。
#include <libgen.h>      // 包含 basename() 函数，用于提取路径中的文件名部分。
#include <11_2_list_timer.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5
static int pipefd[2];
static sort_timer_list timer_list;
static int epollfd =0;

int setnonblocking(int fd){
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;//函数返回原始的标志位值old_option，这样调用者可以知道文件描述符在被设置为非阻塞模式之前的状态。
}

void addfd(int epollfd,int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1],(char*)&msg,1,0);
    errno = save_errno;
}

void addsig(int sig){
    //设置一个信号处理函数，并确保在信号处理函数执行期间，所有信号都被阻塞，以防止信号处理函数被其他信号打断。
    //同时，通过设置SA_RESTART标志，确保某些系统调用在被信号打断后能够自动重启
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |=SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}

void timer_handler(){
    timer_list.tick();
    alarm(TIMESLOT);
}

void cb_func(client_data *user_data){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n",user_data->sockfd);
}

int main(int argc,char *argv[]){
    if(argc<=2){
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET,ip,&address.sin_addr);

    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd >=0 );

    ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    ret = listen(listenfd,5);
    assert(ret!=-1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd,listenfd);

    ret = socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
    assert(ret!=-1);
    setnonblocking(pipefd[1]);
    addfd(epollfd,pipefd[0]);

    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server = false;
    client_data *users = new client_data[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);// 设置定时器，5秒后发送SIGALRM信号

    while(!stop_server){
        int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0)&&errno!=EINTR){
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++){
            int sockfd = events[i].data.fd;
            if(sockfd ==listenfd){
                struct sockaddr_in client_address;
                socklen_t client_addrlen =sizeof(client_address);
                int connfd = accept(listenfd,(struct sockaddr*)&client_address,&client_addrlen);
                addfd(epollfd,connfd);
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;
                util_timer *timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;
                users[connfd].timer = timer;
                timer_list.add_timer(timer);
            }
            else if( (sockfd==pipefd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(pipefd[0],signals,sizeof(signals),0);
                if(ret == -1){
                    //处理
                    continue;
                }
                else if(ret == 0){
                    continue;
                }
                else{
                    for(int i=0;i<ret;i++){
                        switch(signals[i]){
                            case SIGALRM:{
                                timeout = true;
                                break;
                            }
                            case SIGTERM:{
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            else if( events[i].events & EPOLLIN ){
                memset(users[sockfd].buf,'\0',BUFFER_SIZE);
                ret = recv(sockfd,users[sockfd].buf,BUFFER_SIZE-1,0);
                printf("get %d bytes of client data %s from %d \n",ret,users[sockfd].buf,sockfd);
                util_timer *timer = users[sockfd].timer;
                if(ret < 0 ){
                    //如果发生读错误，关连接，删定时器
                    if(errno != EAGAIN){
                        cb_func(&users[sockfd]);
                        if(timer) timer_list.del_timer(timer);
                    }
                }
                else if(ret ==0){
                    cb_func(&users[sockfd]);
                    if(timer) timer_list.del_timer(timer);
                }
                else{
                    //如果有数据可读，调整，延长定时器
                    if(timer){
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        printf("adjust timer once\n");
                        timer_list.adjust_timer(timer);
                    }
                }
            }
            else{}
        }
        if(timeout){
            timer_handler();
            timeout = false;
        }
    }
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete [] users;
    return 0;
}