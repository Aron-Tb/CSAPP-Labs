#include "csapp.c"

void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);

typedef struct {
    int maxfd;            // Read_set 中的最大文件描述符
    fd_set read_set;      // Set  of all active  descriptors
    fd_set ready_set;     // Subset of descriptors ready for reading
    int nready;           // Number of ready descriptors from select 
    int maxi;             // High water index into client array ， 即 clientfd 数组的最大索引，避免从头到尾搜索
    int clientfd[FD_SETSIZE];          // Set of active descriptors
    rio_t clientrio[FD_SETSIZE];       // Set of active read buffers
}pool;

int byte_cnt = 0;  // Counts total bytes received by server

int main(int argc, char **argv) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    static pool pool;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
    // Wait for listening/connected descriptor(s) to become ready
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        // If listening descriptor ready, add new client to pool
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
        }

        // Echo a text line from each ready connected descriptor
        check_clients(&pool);
    }
} 

void init_pool(int listenfd, pool *p) {
    p->maxi = -1;
    // -1 表示可用的描述槽位
    for (int i=0; i<FD_SETSIZE; ++i) 
        p->clientfd[i] = -1;
    
    // 初始化时， listenfd 是唯一 select read set
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p) {
    int i;
    p->nready--;  // ?
    // Find an available slot
    for (i=0; i<FD_SETSIZE; ++i) {
        if (p->clientfd[i] < 0) {
            // Add connected descriptor to the pool
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            // ADD the descriptor to descriptor set
            FD_SET(connfd, &p->read_set);

            // Update max descriptor and pool high water mark
            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }
    if (i == FD_SETSIZE)
        app_error("add_client error: Too many clients");
}

void check_clients(pool *p) {
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i=0; (i<=p->maxi) && (p->nready>0); ++i) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        // If the descriptor  is ready, echo a text line from it
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE) ) != 0 ) {
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
                Rio_writen(connfd, buf, n);
            }
            // EOF
            else {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}
