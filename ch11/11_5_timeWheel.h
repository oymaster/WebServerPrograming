#ifndef TIME_WHEEL
#define TIME_WHELL

#include<time.h>
#include<netinet/in.h>
#include<stdio.h>

#define BUFFER_SIZE 64
class tw_timer;

struct client_data{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    tw_timer *timer;
};

class tw_timer{//插入时间轮的时间器
public:
    int rotation;//记录定时器在时间轮转多少圈后生效
    int time_slot;//对应时间轮的槽
    void (*cb_func)(client_data *);
    client_data *user_data;
    tw_timer *next;
    tw_timer *prev;
    tw_timer(int rot,int ts): next(NULL),prev(NULL),rotation(rot),time_slot(ts){};
};

class time_wheel{//时间轮结构
public:
    time_wheel();
    ~time_wheel();
    tw_timer *add_timer(int timeout);
    void del_timer(tw_timer *timer);
    void tick();//SI时间一到，开启心跳，转动时间轮

private:
    static const int N = 60;//时间轮槽数
    static const int SI = 1;//时间轮时间间隔
    tw_timer *slots[N];
    int cur_slot;//当前所在槽数
};

#endif 