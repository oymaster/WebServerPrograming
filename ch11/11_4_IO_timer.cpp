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
#define TIMEOUT 5000

int main(){
    int timeout =TIMEOUT;
    time_t start =time(NULL);
    time_t end = time (NULL);
    while(1){
        printf("the timeout is now %d mil-secons\n",timeout);
        start = time (NULL);
        int number = epoll_wait(epollfd,events,MAX_EVENTS_NUMBER,timeout);
        if((number<0)&&(errno != EINTR)){
            printf("epoll failure\n");
            break;
        }
        if(number ==0 ){
            timeout = TIMEOUT;
            continue;
        }
        end = time(NULL);
        timeout -= (end - start) * 1000;
        if(timeout <= 0) timeout = TIMEOUT;
        
    }
}