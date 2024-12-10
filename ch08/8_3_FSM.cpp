#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#define BUFFER_SIZE 4096
enum CHECK_STATE{
    CHECK_STATE_REQUSETLINE=0,//分析请求行
    CHECK_STATE_HEADER        //分析头部字段
};
enum LINE_STATUS{
    LINE_OK=0,                //读到完整的行
    LINE_BAD,                 //行出错
    LINE_OPEN                 //行数据尚且不完整
};
enum HTTP_CODE{
    NO_REQUEST,               //请求不完整
    GET_REQUEST,              //获得了一个完整的客户请求
    BAD_REQUEST,              //请求有错误    FORBIDDEN_REQUSET,        //无足够访问权限
    INTERNAL_ERROR,           //服务器内部错误
    COLSE_CONNECTION          //关闭连接
};
static const char* szert[] = {"I get a correct result\n","something wrong\n"};
//从状态机，用于解析一行内容
LINE_STATUS parese_line(char* buffer,int& checked_index,int&read_index){
    char temp;
    /*checked_index指向buffer（应用程序的读缓冲区）中当前正在分析的字节，
    read_index指向buffer中客户数据的尾部的下一字节，buffer中第0~checked_index字节都已分析完毕，第checked_
    index~（read_index-1）字节由下面的循环挨个分析*/
    for(;checked_index<read_index;++checked_index){
        temp = buffer[checked_index];
        if(temp=='\r'){
            if((checked_index+1)==read_index) return LINE_OPEN;
            else if(buffer[checked_index+1]=='\n'){
                buffer[checked_index++]='\0';
                buffer[checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n'){
            if((checked_index>1)&& buffer[checked_index-1] == '\r'){
                buffer[checked_index-1]='\0';
                buffer[checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN; 
}
//分析请求行
HTTP_CODE parse_requestline(char* temp,CHECK_STATE& checkstate){
    char* url=strpbrk(temp,"\t");//依次检验字符串 str1 中的字符，当被检验字符在字符串 str2 中也包含时，则停止检验，并返回该字符位置。
    //如果请求行中没有空白字符或者\t则请求有问题
    if(!url){
        return BAD_REQUEST;
    }
    *url++='\0';
    char* method =temp;
    if(strcasecmp(method,"GET")==0) printf("The requset method is GET\n");
    else return BAD_REQUEST;

    url+=strspn(url,"\t");
    char* version=strpbrk(url,"\t");
    if(!version){
        return BAD_REQUEST;
    }
    *version++='\0';
    version +=strspn(version,"\t");
    //仅支持http1.1
    if(strcasecmp(version,"HTTP/1.1")!=0) return BAD_REQUEST;
    if(strncasecmp(url,"http://",7)==0){
        url +=7;
        url = strchr(url,'/');
    }
    if(!url || url[0]!='/' )return BAD_REQUEST;
    printf("The request URL is: %s\n",url);
    checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;

}
//分析头部
HTTP_CODE parse_headers(char* temp){
    if(temp[0]=='\0') return GET_REQUEST;
    else if(strncasecmp(temp,"Host:",5)==0){
        temp+=5;
        temp+=strspn(temp,"\t");
        printf("the request host is: %s\n",temp);
    }
    else{
        printf("I can not handle this header\n");
    }
    return NO_REQUEST;
}

//分析http请求的入口函数
HTTP_CODE parse_content(char* buffer,int& checked_index,CHECK_STATE checkstate,int& read_index,int& start_line){
    LINE_STATUS linestatus= LINE_OK;
    HTTP_CODE retcode=NO_REQUEST;
    while((linestatus=parese_line(buffer,checked_index,read_index))==LINE_OK){
        char* temp=buffer+start_line;
        switch (checkstate)
        {
            case CHECK_STATE_REQUSETLINE:
                retcode=parse_requestline(temp,checkstate);
                if(retcode==BAD_REQUEST) return BAD_REQUEST;
                break;
            case CHECK_STATE_HEADER:
                retcode=parse_headers(temp);
                if(retcode==BAD_REQUEST) return BAD_REQUEST;
                else if(retcode == GET_REQUEST) return GET_REQUEST;
                break;
            default:
                return INTERNAL_ERROR;
                break;
        }
    }
    if(linestatus==LINE_OPEN) return NO_REQUEST;
    else return BAD_REQUEST;
}

int main(int argc,char* argv[]){
    if(argc<=2){
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    //监听的端口和ip地址
    const char *ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address,sizeof(address));//赋0
    address.sin_family=AF_INET;//ipv4地址协议族
    inet_pton(AF_INET,ip,&address.sin_addr);//将地址从易懂的字符串转为网络字节序
    address.sin_port=htons(port);//主机字节序转网络短字节序
 
    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);
    int ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));  
    ret=listen(listenfd,5);
    assert(ret!=-1);
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int fd=accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
    if(fd<0) printf("errno is %d\n",errno);
    else{
        char buffer[BUFFER_SIZE];
        memset(buffer,'\0',BUFFER_SIZE);
        int data_read=0;
        int read_index=0;
        int checked_index=0;
        int start_line=0;
        CHECK_STATE checkstate = CHECK_STATE_REQUSETLINE;
        while(1){
            data_read=recv(fd,buffer+read_index,BUFFER_SIZE-read_index,0);
            if(data_read==-1){
                printf("reading failed\n");
                break;
            }
            else if(data_read==0){
                printf("remote client has colsed the connection\n");
                break;
            }
            read_index+=data_read;
            HTTP_CODE result = parse_content(buffer,checked_index,checkstate,read_index,start_line);
            if(result == NO_REQUEST) continue;
            else if(result == GET_REQUEST){
                send(fd,szert[0],strlen(szert[0]),0);
                break;
            }
            else{
                send(fd,szert[1],strlen(szert[1]),0);
                break;
            }
        }
        close(fd);
    }
    close(listenfd);
    return 0;
}