#ifndef THREAD_POOL
#define THREAD_POOL

#include "csapp.h"

#define NTHREADS 100  // max number of threads

typedef struct {
    sem_t mutex; 
    sem_t slots;     // 空闲槽数量
    sem_t free_fds;  // 需要被线程服务的连接数
    int fd_set[NTHREADS];
    int fd_free[NTHREADS];  // 标记连接是否正在被服务: 0正在被服务、1未被服务、-1不存在连接(fd_set = -1)
}Pool;

// Thread_pool functions
void init_pool(Pool *p);
void add_client(int connfd, Pool *p);
void remove_client(int connfd, Pool *p);
void *thread(void *vargp);
#endif 