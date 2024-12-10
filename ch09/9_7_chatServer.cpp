#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdio.h>
#include<poll.h>
#include<fcntl.h>
#include<stdlib.h>
#include<errno.h>
#define _GNUSOURCE 1
#define BUFFER_SIZE 64
#define USER_LIMIT 5
#define FD_LIMIT 65535

struct client_data{
    sockaddr_in address;
    char *write_buf;
    char buf[BUFFER_SIZE];
};

int setnonblocking(int fd){
    //FL file status flag
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

int main(int argc,char *argv[]){
    if(argc<=2){
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char *ip=argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_port=htons(port);
    inet_pton(AF_INET,ip,&address.sin_addr);

    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);

    ret=bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    ret = listen(listenfd,5);
    assert(ret!=-1);

    client_data *users = new client_data[FD_LIMIT];
    pollfd fds[USER_LIMIT+1];
    int user_counter =0;
    for(int i=1;i<=USER_LIMIT;++i){
        fds[i].fd=-1;
        fds[i].events=0;
    }
    fds[0].fd=listenfd;
    fds[0].events=POLLIN | POLLERR;
    fds[0].revents = 0;

    while(1){
        ret = poll(fds,user_counter+1,-1);
        if(ret < 0){
            printf("poll failure\n");
            break;
        }
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
        for(int i=0;i<user_counter+1;++i){
            //监听，建立连接
            if(fds[i].fd==listenfd && fds[i].revents &POLLIN){
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
                if(connfd < 0 ){
                    printf("errno is %d\n",errno);
                    continue;
                }            
                if(user_counter>= USER_LIMIT){
                    const char *info= "too many users\n";
                    printf("%s\n",info);
                    send(connfd,info,strlen(info),0);
                    close(connfd);
                    continue;
                }    
                
                user_counter++;
                users[connfd].address = client_address;
                setnonblocking(connfd);
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
                printf("comes a new user, now have %d users\n",user_counter);
            }
            //出错
            else if(fds[i].revents & POLLERR){
                printf("get an error from %d\n",fds[i].fd);
                char errors[100];
                memset(errors,'\0',100);
                socklen_t length = sizeof(errors);
                if(getsockopt(fds[i].fd,SOL_SOCKET,SO_ERROR,&errors,&length)<0)
                    printf("get socket option failed\n");
                continue;
            }
            //客户端关闭连接
            else if(fds[i].revents & POLLRDHUP){
                users[fds[i].fd]= users[fds[user_counter].fd];
                close(fds[i].fd);
                fds[i] = fds[user_counter];
                i--;
                user_counter--;
                printf("a client left\n");
            }
            //处理可读连接
            else if(fds[i].revents & POLLIN){
                int connfd = fds[i].fd;
                memset(users[connfd].buf,'\0',BUFFER_SIZE);
                ret = recv(connfd, users[connfd].buf,BUFFER_SIZE-1,0);
                printf("get %d bytes of client data %s from %d\n",ret,users[connfd].buf,connfd);
                if(ret<0){
                    if(errno!=EAGAIN){
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }
                else if(ret ==0){}
                else {
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET,&users[connfd].address.sin_addr,client_ip,INET_ADDRSTRLEN);
                    int client_port = ntohs(users[connfd].address.sin_port);
                    char formatted_msg[BUFFER_SIZE + 50]; // 足够容纳附加信息
                    snprintf(formatted_msg, sizeof(formatted_msg), "User[%d] (%s:%d): %s", connfd, client_ip, client_port,users[connfd].buf);
                    for(int j=1;j<=user_counter;++j){
                        if(fds[j].fd==connfd){
                            continue;
                        }
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = strdup(formatted_msg);
                    }
                }
            }
            else if(fds[i].revents & POLLOUT){
                int connfd = fds[i].fd;
                if(!users[connfd].write_buf) continue;
                ret = send(connfd,users[connfd].write_buf,strlen(users[connfd].write_buf),0);
                free(users[connfd].write_buf);
                users[connfd].write_buf = NULL;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }
    delete [] users;
    close(listenfd);
    return 0;
}