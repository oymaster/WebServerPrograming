#include<sys/sem.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>

union semun
{
    int val;                   // 设置单个信号量值
    struct semid_ds *buf;      // 信号量集合的属性结构体
    unsigned short int *array; // 信号量集合所有信号量的值
    struct seminfo *_buf;      // 内核信号量信息
};

void pv(int sem_id, int op){
    struct sembuf sem_b;
    sem_b.sem_flg = SEM_UNDO;
    sem_b.sem_num = 0;
    sem_b.sem_op = op;
    semop(sem_id,&sem_b,1);
}


int main(int argc, char *argv[]){
    int sem_id = semget(IPC_PRIVATE,1,0666);
    union semun sem_un;
    sem_un.val = 1;
    semctl(sem_id,0,SETVAL,sem_un);

    pid_t id = fork();
    if(id<0){
        return 1;
    }
    else if(id == 0){
        printf("child try to get binary sem\n");
        pv(sem_id,-1);
        printf("child get the sem and would release it after 5 seconds\n");
        sleep(5);
        pv(sem_id,1);
        exit(0);
    }
    else{
        printf("parent try to get binary sem\n");
        pv(sem_id,-1);
        printf("parent get the sem and would release it after 5 seconds\n");
        sleep(5);
        pv(sem_id,1);
    }
    waitpid(id,NULL,0);
    semctl(sem_id,0,IPC_RMID,sem_un);//删除信号量
    return 0;
}
