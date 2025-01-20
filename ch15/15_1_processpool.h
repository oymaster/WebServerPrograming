#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

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

/* 
 * 子进程信息类
 * 用于存储和管理子进程的相关信息
 * 包括：
 * - 子进程PID
 * - 父子进程通信的管道
 */
class process{
public:
    process() : m_pid(-1){}

public:
    pid_t m_pid;        // 子进程PID
    int m_pipefd[2];    // 父子进程通信的管道
};

/* 
 * 进程池模板类
 * T: 处理具体任务的逻辑类，必须实现init和process方法
 */
template<typename T>
class processpool{
private:
    processpool(int listenfd,int processs_number = 8);

public:
    /* 
     * 创建进程池实例（单例模式）
     * listenfd: 监听socket文件描述符
     * process_number: 子进程数量，默认为8
     */
    static processpool<T> *create(int listenfd, int process_number = 8){
        if(!m_instance) m_instance = new processpool<T>(listenfd,process_number);
        return m_instance;
    }
    ~processpool() {delete [] m_sub_process; }
    
    /* 
     * 运行进程池
     * 根据当前进程是父进程还是子进程调用相应的处理函数
     */
    void run();

private:
    /* 
     * 设置信号处理管道
     * 创建epoll实例和信号管道，并添加信号处理函数
     */
    void setup_sig_pipe();
    /* 
     * 父进程运行逻辑
     * 1. 设置信号处理管道
     * 2. 监听连接请求
     * 3. 将新连接分配给子进程处理
     * 4. 处理子进程状态变化
     */
    void run_parent();
    /* 
     * 子进程运行逻辑
     * 1. 设置信号处理管道
     * 2. 监听来自父进程的管道消息
     * 3. 处理客户端连接和请求
     */
    void run_child();

private:
    static const int MAX_PROCESS_NUMBER = 16;  // 最大子进程数量
    static const int USER_PER_PROCESS = 65536; // 每个子进程最多处理的用户数
    static const int MAX_EVENT_NUMBER = 10000; // epoll最多处理的事件数
    int m_process_number;  // 子进程总数
    int m_idx;             // 子进程在池中的序号，-1表示父进程
    int m_epollfd;         // 每个进程都有一个epoll内核事件表
    int m_listenfd;        // 监听socket，用于接收客户端连接
    int m_stop;            // 是否停止运行标志位
    process * m_sub_process; // 子进程信息数组，存储所有子进程信息
    static processpool<T> *m_instance; // 进程池单例，保证全局唯一
};

template<typename T>
processpool<T> * processpool<T>::m_instance = nullptr;

static int sig_pipefd[2];

static int setnonblocking(int fd){
    int old_op = fcntl(fd,F_GETFL);
    int new_op = old_op | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_op);
    return old_op;
}

static void addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

static void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

static void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1],(char *)&msg,1,0);
    errno = save_errno;
}

static void addsig(int sig, void(handler)(int),bool restart = true){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    if(restart) sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}

template<typename T>
processpool<T>::processpool(int listenfd,int process_number)
    :m_listenfd(listenfd),m_process_number(process_number),m_idx(-1),m_stop(false)
{
    assert((process_number>0) && (process_number<=MAX_PROCESS_NUMBER));

    m_sub_process = new process[process_number];
    assert(m_sub_process);

    for(int i=0;i<process_number;++i){
        int ret = socketpair(PF_UNIX,SOCK_STREAM,0,m_sub_process[i].m_pipefd);
        assert(ret == 0);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid>=0);
        if(m_sub_process[i].m_pid > 0){
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else{
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

template<typename T>
void processpool<T>::setup_sig_pipe(){
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    int ret = socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);
    assert(ret != -1);

    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd,sig_pipefd[0]);

    addsig(SIGCHLD,sig_handler);
    addsig(SIGTERM,sig_handler);
    addsig(SIGINT,sig_handler);
    addsig(SIGPIPE,SIG_IGN);
}

template<typename T>
void processpool<T>::run(){
    if(m_idx != -1){
        run_child();
        return;
    }
    run_parent();
}

template<typename T>
void processpool<T>::run_child(){
    setup_sig_pipe();

    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    addfd(m_epollfd,pipefd);

    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    assert(users);

    int number =0;
    int ret = -1;
    while(!m_stop){
        number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && errno !=EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i=0; i<number; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == pipefd && events[i].events &EPOLLIN){
                int client = 0;
                ret = recv(sockfd,(char *)&client,sizeof(client),0);
                if((ret < 0 && errno != EAGAIN) || ret == 0){
                    continue;
                }
                else{
                    struct sockaddr_in client_address;
                    socklen_t client_addrlength = sizeof(client_address);
                    int connfd = accept(m_listenfd,(struct sockaddr *)&client_address,&client_addrlength);
                    if(connfd < 0){
                        printf("errno is %d\n",errno);
                        continue;
                    }
                    addfd(m_epollfd,connfd);
                    //模板类T必须实现init方法，以初始化一个客户连接。我们直接使用connfd来索引逻辑处理对象（T类型的对象），以提高程序效率
                    users[connfd].init(m_epollfd,connfd,client_address);
                }
            }
            else if(sockfd == sig_pipefd[0] && events[i].events & EPOLLIN){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0],signals,sizeof(signals),0);
                if(ret <= 0 ) continue;
                else{
                    for (int j = 0; j < ret; ++j) {  // 使用不同的变量名 j
                        switch (signals[j]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    continue;  // 可以在这里处理进程回收的逻辑
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT: {
                                m_stop = true;
                                break;
                            }    
                            default:
                                break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN) users[sockfd].process(); // 处理用户请求
            else continue; // 处理其他事件
        }
    }

    delete [] users;
    users = NULL;
    close(pipefd);
    //close(m_lisenfd) 这里不应该销毁，谁创建它谁负责销毁
    close(m_epollfd);
}

template<typename T>
void processpool<T>::run_parent(){
    setup_sig_pipe();
    //父进程监听连接
    addfd(m_epollfd,m_listenfd);

    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn =1;
    int number =0;
    int ret = -1;

    while(!m_stop){
        number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && errno !=EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i=0; i<number; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd){// 有新的连接
                int j = sub_process_counter;  // 使用不同的变量名 j
                do {
                    if (m_sub_process[j].m_pid != -1) {
                        break;
                    }
                    j = (j + 1) % m_process_number;
                } while (j != sub_process_counter);

                if (m_sub_process[j].m_pid == -1) {
                    m_stop = true;
                    break;
                }
                sub_process_counter = (j + 1) % m_process_number;
                send(m_sub_process[j].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                printf("send request to child %d\n", j);
            }
            // 处理信号
            else if(sockfd == sig_pipefd[0] && events[i].events & EPOLLIN){ 
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0],signals,sizeof(signals),0);//  从信号管道中接收信号
                if(ret <= 0 ) continue;
                else{
                    for(int j=0 ; j < ret ; ++j ){
                        switch (signals[j])
                        {
                        case SIGCHLD:{//  子进程退出
                            pid_t pid;
                            int stat;
                            while( (pid = waitpid(-1,&stat,WNOHANG))>0){
                                for(int k=0;k<m_process_number;++k){
                                    if(m_sub_process[k].m_pid == pid){
                                        printf("child %d join\n",k);
                                        close(m_sub_process[k].m_pipefd[0]);
                                        m_sub_process[k].m_pid = -1;
                                    }
                                }
                            }
                            m_stop = true;
                            for(int k=0 ;k<m_process_number;++k){
                                if(m_sub_process[k].m_pid != -1) m_stop = flase;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:{//  父进程终止
                            printf("kill all the child now \n");
                            for(int k=0;k<m_process_number;++k){
                                int pid = m_sub_process[k].m_pid;
                                if(pid !=-1){
                                    kill(pid,SIGTERM);
                                }
                            }
                            break;
                        }    
                        default:
                            break;
                        }
                    }
                }
            }
            else continue;
        }
    }

    //close(m_lisenfd) 这里不应该销毁，谁创建它谁负责销毁
    close(m_epollfd);
}

#endif
