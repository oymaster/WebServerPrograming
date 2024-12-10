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


int main(int argc, char *argv[]) {
    
    if (argc != 2){
        printf("usage: %s <file>\n", basename(argv[0]));
        return 1;
    }
    int filefd=open(argv[1],O_CREAT|O_WRONLY|O_TRUNC,0666);
    assert(filefd);

    int pipefd_stdout[2];
    int ret = pipe(pipefd_stdout);
    assert(ret!=-1);

    int pipefd_file[2];
    ret = pipe(pipefd_file);
    assert(ret!=-1);
    //从标准输入中移到pipe_stdout,pipe_file复制,然后再将pipefile移动到标准输出中
    ret=splice(STDIN_FILENO,NULL,pipefd_stdout[1],NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
    assert(ret!=-1);

    ret=tee(pipefd_stdout[0],pipefd_file[1],32768,SPLICE_F_NONBLOCK);
    assert(ret!=-1);

    ret=splice(pipefd_file[0],NULL,filefd,NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
    assert(ret!=-1);
    
    ret=splice(pipefd_stdout[0],NULL,STDOUT_FILENO,NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
    assert(ret!=-1);

    close(filefd);
    close(pipefd_file[0]);
    close(pipefd_file[1]);
    close(pipefd_stdout[0]);
    close(pipefd_stdout[1]);

    return 0;
}
