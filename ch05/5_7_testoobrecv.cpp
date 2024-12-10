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
#define BUF_SIZE 1024
//recv
int main(int argc,char *argv[] ){
    if(argc<=2){
        printf("usage: %s ip_address port_number backlog\n", basename(argv[0]));
        return 1;
    }
    const char * ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);//host to short network
    
    int sockfd=socket(PF_INET,SOCK_STREAM,0);
    assert(sockfd>=0);

    int ret = bind(sockfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    ret=listen(sockfd,5);
    assert(ret!=-1);
    
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sockfd,(struct sockaddr*)&client,&client_addrlength);
    if(connfd<0) printf("errno is %d\n",errno);
    else{
        char buffer[BUF_SIZE];
        memset(buffer,'\0',BUF_SIZE);
        ret = recv(connfd,buffer,BUF_SIZE-1,0);
        printf("got %d bytes of normal data '%s'\n",ret,buffer);

        memset(buffer,'\0',BUF_SIZE);
        ret = recv(connfd,buffer,BUF_SIZE-1,MSG_OOB);
        printf("got %d bytes of OOB data '%s'\n",ret,buffer);

        memset(buffer,'\0',BUF_SIZE);
        ret = recv(connfd,buffer,BUF_SIZE-1,0);
        printf("got %d bytes of normal data '%s'\n",ret,buffer);
        close(connfd);
    }

    close(sockfd);
    return 0;
}