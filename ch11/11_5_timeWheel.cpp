#include "11_5_timeWheel.h"
time_wheel::time_wheel(): cur_slot(0){
    //初始化
    for(int i=0;i<N;i++){
        slots[i] = NULL;
    }
}
time_wheel::~time_wheel(){
    //初始化
    for(int i=0;i<N;i++){
        tw_timer *tmp = slots[i];
        while(tmp){
            slots[i] = tmp->next;
            delete tmp;
            tmp = slots[i];
        }
    }
}

tw_timer *time_wheel::add_timer(int timeout){
    if( timeout < 0 ) return NULL;
    int ticks =0;//计算多少个槽后触发
    if( timeout < SI) 
        ticks = 1;
    else 
        ticks = timeout / SI;
    //转多少圈
    int rotation = ticks / N;
    int ts = (cur_slot + (ticks & N)) %N;//计算具体存放的第几个槽
    tw_timer *timer = new tw_timer(rotation,ts);
    if(!slots[ts]){
        printf("add timer,rotation is %d,ts is %d,cur_slot is %d\n",rotation,ts,cur_slot);
        slots[ts] = timer;
    }
    else{
        //插入链表头
        timer->next = slots[ts];
        slots[ts]->prev = timer;
        slots[ts] = timer;
    }
    return timer;
}


void time_wheel::del_timer(tw_timer* timer){
    if(!timer) return;
    int ts = timer->time_slot;
    if(timer == slots[ts]){
        //如果是头部
        slots[ts] = slots[ts]->next;
        //如果后面还有时间器
        if(slots[ts]) slots[ts]->prev = NULL;
        delete timer;
    }else{
        timer->prev->next = timer->next;
        if(timer->next)
            timer->next->prev = timer->prev;
        delete timer;
    }
}

void time_wheel::tick(){
    tw_timer *tmp = slots[cur_slot];
    printf("curren slot is %d\n",cur_slot);
    while(tmp){
        printf("tick the timer once\n");
        if(tmp->rotation > 0){
            tmp->rotation--;
            tmp = tmp ->next;
        }
        else{
            tmp->cb_func(tmp->user_data);
            if(tmp == slots[cur_slot]){
                printf("delete header in cur_slot\n");
                slots[cur_slot] = tmp ->next;
                delete tmp;
                if(slots[cur_slot]) slots[cur_slot]->prev = NULL;
                tmp = slots[cur_slot];
            }
            else{
                tmp->prev->next = tmp->next;
                if(tmp->next)
                    tmp->next->prev = tmp->prev;
                tw_timer *nx = tmp->next;
                delete tmp;
                tmp = nx;
            }
        }
    }
    ++cur_slot;
    cur_slot %= N;
}
