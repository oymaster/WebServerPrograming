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
int main(int argc,char *argv[] ){
    if(argc<=2){
        printf("usage: %s ip_address port_number backlog\n", basename(argv[0]));
        return 1;
    }
    int sock=socket(PF_INET,SOCK_STREAM,0);
    assert(sock>=0);
    int reuse=1;
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    const char * ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);//host to short network
    
    int ret=bind(sock,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);  


    close(sock);
    return 0;
}