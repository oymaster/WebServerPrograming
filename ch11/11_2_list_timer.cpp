#include "11_2_list_timer.h"

sort_timer_list::~sort_timer_list(){
    util_timer *tmp=head;
    while(tmp){
        head=tmp->next;
        delete head;
        tmp = head;
    }
}

void sort_timer_list::add_timer(util_timer *timer){
    if(!timer) return;
    //头节点为空
    if(!head){
        head=tail=timer;
        return;
    }
    //如果小于头节点的时间
    if( timer->expire < head->expire ){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer,head);
}

void sort_timer_list::add_timer(util_timer *timer,util_timer *list_timer){
    util_timer *pre = list_timer;
    util_timer *tmp = pre->next;
    while(tmp){
        if(timer->expire < tmp->expire ){
            pre->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = pre;
            break;
        }
        pre = tmp;
        tmp = tmp->next;
    }
    if(!tmp){
        pre->next = timer;
        timer->prev = pre;
        timer->next = NULL;
        tail = timer;
        return;
    }
}

void sort_timer_list::adjust_timer(util_timer *timer){
    if(!timer) return;
    util_timer *tmp = timer->next;
    if(!tmp || timer->expire < tmp->expire) return;
    if(timer == head){
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer,head);
    }
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer,head); 
    }
}

void sort_timer_list::del_timer(util_timer *timer){
    if(!timer) return;
    if(timer == head && timer==tail ) {
        delete timer;
        head = tail = NULL;
        return;
    }
    if(timer == head){
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_list::tick(){
    if(!head) return;
    printf("timer tick\n");
    time_t cur = time(NULL);//获取当前时间
    util_timer *tmp =head;
    while(tmp){
        if(cur < tmp->expire) break;
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if(head){
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}