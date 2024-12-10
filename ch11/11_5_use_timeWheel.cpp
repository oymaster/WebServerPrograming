#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <libgen.h>
#include "11_5_timeWheel.h" // 包含时间轮的定义
#include <fcntl.h>
#include <sys/epoll.h>

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static time_wheel timer_wheel; // 时间轮实例
static int epollfd = 0;

// 设置文件描述符非阻塞
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将文件描述符添加到 epoll 监听中
void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数
void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号处理
void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时器回调函数
void cb_func(client_data* user_data) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

// 定时器信号处理函数
void timer_handler() {
    timer_wheel.tick();
    alarm(TIMESLOT);
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server = false;
    client_data* users = new client_data[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);

    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);
                addfd(epollfd, connfd);
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;

                // 创建定时器并加入时间轮
                tw_timer* timer = timer_wheel.add_timer(3 * TIMESLOT);
                if (timer) {
                    timer->cb_func = cb_func;
                    timer->user_data = &users[connfd];
                    users[connfd].timer = timer;
                }
            }
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0) continue;
                for (int i = 0; i < ret; i++) {
                    switch (signals[i]) {
                        case SIGALRM:
                            timeout = true;
                            break;
                        case SIGTERM:
                            stop_server = true;
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
                tw_timer* timer = users[sockfd].timer;

                if (ret <= 0) {
                    cb_func(&users[sockfd]);
                    if (timer) timer_wheel.del_timer(timer);
                }
                else {
                    printf("get %d bytes of client data: %s\n", ret, users[sockfd].buf);
                    if (timer) {
                        timer_wheel.del_timer(timer);
                        timer = timer_wheel.add_timer(3 * TIMESLOT);
                        if (timer) {
                            timer->cb_func = cb_func;
                            timer->user_data = &users[sockfd];
                            users[sockfd].timer = timer;
                        }
                    }
                }
            }
        }

        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }

    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    return 0;
}
