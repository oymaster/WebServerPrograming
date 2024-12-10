#ifndef LIST_TIMER
#define LIST_TIMER

#include <time.h>
#include <netinet/in.h>
#define BUFFER_SIZE 64
class util_timer;

struct client_data{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;
};

class util_timer{
public:
    util_timer(): prev(NULL),next(NULL){};

    time_t expire;//超时时间，绝对时间
    void (*cb_func) (client_data*);//回调函数
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

class sort_timer_list{
private:
    util_timer *head;
    util_timer *tail;
    void add_timer(util_timer *timer,util_timer *list_head);

public:
    sort_timer_list():head(NULL),tail(NULL){};
    ~sort_timer_list();
    void add_timer(util_timer *timer);
    //当某个定时器任务发生变化的时候，调整这个定时器的位置
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer* timer);
    //SIGALRM信号每次被触发的就在信号处理函数中调用该tick函数，用于处理到期任务
    void tick();
};

#endif