#include "csapp.h"
#include "thread_pool.h"

void init_pool(Pool *p) {
    Sem_init(&p->mutex, 0, 1);
    Sem_init(&p->slots, 0, NTHREADS);
    Sem_init(&p->free_fds, 0, 0);
    for (int i=0; i<NTHREADS; ++i) {
        p->fd_set[i] = -1;
        p->fd_free[i] = -1;
    }
}

void add_client(int connfd, Pool *p) {
    P(&p->slots);
    P(&p->mutex);
    for (int i=0; i<NTHREADS; ++i) {
        if (p->fd_set[i] < 0) {
            p->fd_set[i] = connfd;
            p->fd_free[i] = 1;
            V(&p->free_fds);
            break;
        }
    }
    V(&p->mutex);
}

void remove_client(int connfd, Pool *p) {
    P(&p->mutex);
    int i;
    for (i=0; i<NTHREADS; ++i) {
        if (p->fd_set[i] == connfd) {
            p->fd_set[i] = -1;
            V(&p->slots);
            break;
        }
    }
    V(&p->mutex);
    if (i == NTHREADS)
        app_error("No such connfd!");
}

// 从线程池返回一个未被线程服务的连接
int get_fd(Pool *p) {
    int connfd;
    P(&p->free_fds);
    P(&p->mutex);
    for (int i=0; i<NTHREADS; ++i) {
        if (p->fd_free[i] == 1) {
            connfd = p->fd_set[i];
            p->fd_free[i] = 0;
            break;
        }
    }
    V(&p->mutex);
    return connfd;
}

extern void doit(int connfd);
extern Pool pool;
void *thread(void *vargp) {
    pthread_t self_tid = pthread_self(); 
    pthread_detach(self_tid);
    int connfd;
    connfd = get_fd(&pool);
    doit(connfd);
    Close(connfd);
    remove_client(connfd, &pool); // 释放线程池
    return NULL;
}
