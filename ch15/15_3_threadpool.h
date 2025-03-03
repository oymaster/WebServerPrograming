#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<cstdio>
#include<exception>//异常处理
#include<pthread.h>//线程
#include"14_2_locker.h"//线程同步机制

template<typename T>
class threadpool{
private:
    int m_thread_number;//线程池中的线程数
    int m_max_requests;//请求队列中允许的最大请求数
    pthread_t *m_threads;//描述线程池的数组，大小为m_thread_number
    std::list<T *> m_workqueue;//请求队列
    locker m_queuelocker;//保护请求队列的互斥锁
    sem m_queuestat;//是否有任务需要处理
    bool m_stop;//是否结束线程

public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    // 往请求队列中添加任务
    bool append(T *request);
    
private:
    static void *worker(void *arg);
    void run();

};


template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests)
    :m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_threads(NULL)
{
    if((thread_number<=0) || (max_requests<=0)){
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }

    // 创建thread_number个线程，并将它们设置为脱离线程
    for(int i=0;i<thread_number;++i){
        printf("create the %dth thread\n",i);
        if(pthread_create(m_threads+i,NULL,worker,this) != 0){
            delete [] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool(){
    delete [] m_threads;
    m_stop = true;
}

template<typename T> 
bool threadpool<T>::append(T *request){
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void *threadpool<T>::worker(void *arg){
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();  // 等待有任务的到来，信号量控制阻塞
        m_queuelocker.lock();  // 上锁，确保对队列的访问是线程安全的
        if (m_workqueue.empty()) {  // 如果任务队列为空，跳过
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();  // 从队列中取任务
        m_workqueue.pop_front();  // 弹出队列头部任务
        m_queuelocker.unlock();  // 解锁队列，其他线程可以继续操作队列
        if(!request){
            continue;
        }
        request->process();
    }
}

#endif