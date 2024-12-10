#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>

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
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
    if (connfd < 0) {
        printf("errno is %d\n", errno);
        close(listenfd);
        return 1;
    }

    // pollfd 数组，包含要监视的文件描述符及其事件
    struct pollfd fds[2];
    fds[0].fd = connfd;
    fds[0].events = POLLIN;    // 普通数据的读事件
    fds[1].fd = connfd;
    fds[1].events = POLLPRI;   // 带外数据的异常事件

    char buf[1024];
    while (1) {
        ret = poll(fds, 2, -1);  // 无限等待，直到有事件发生
        if (ret < 0) {
            printf("poll failure\n");
            break;
        }

        // 检查是否有普通数据可读
        if (fds[0].revents & POLLIN) {
            memset(buf, '\0', sizeof(buf));
            ret = recv(connfd, buf, sizeof(buf) - 1, 0);
            if (ret <= 0) {
                printf("client disconnected\n");
                break;
            }
            printf("get %d bytes of normal data: %s\n", ret, buf);
        }

        // 检查是否有带外数据（OOB）可读
        if (fds[1].revents & POLLPRI) {
            memset(buf, '\0', sizeof(buf));
            ret = recv(connfd, buf, sizeof(buf) - 1, MSG_OOB); // 读取带外数据
            if (ret <= 0) {
                printf("client disconnected\n");
                break;
            }
            printf("get %d bytes of OOB data: %s\n", ret, buf);
        }
    }

    close(connfd);
    close(listenfd);
    return 0;
}
