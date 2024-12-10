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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/sendfile.h>
#include <libgen.h>      // 包含 basename() 函数，用于提取路径中的文件名部分。
#define BUFFER_SIZE 1024

static const char* status_line[2]={"200 OK","500 Internal server error"};

int main(int argc, char *argv[]) {
    
    if (argc <= 2){
        printf("usage: %s ip_address port_number backlog\n", basename(argv[0]));
        return 1;
    }
    
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    
    ret = listen(sock, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_addrlength=sizeof(client);
    int connfd = accept(sock,(struct sockaddr*)&client,&client_addrlength);
    if(connfd<0) printf("errno is :%d\n",errno);
    else{
        //连接成功，打印客户端ip地址和端口号
        char remote[INET_ADDRSTRLEN];
        printf("connected with ip: %s and port: %d\n",inet_ntop(AF_INET,&client.sin_addr,remote,INET_ADDRSTRLEN),ntohs(client.sin_port));
        int pipefd[2];
        assert(ret!=-1);
        ret =pipe(pipefd);//创建管道
        //将connfd中的数据定向到管道中
        ret=splice(connfd,NULL,pipefd[1],NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
        assert(ret!=-1);
        // 将管道的数据定向到connfd客户端中
        ret=splice(pipefd[0],NULL,connfd,NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
        assert(ret!=-1);
        close(connfd);
    }

    close(sock);
    return 0;
}
