#include "csapp.h"

/* Basic Part */

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
struct Url {
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
};
// Basic function's  prototypes
void sigpipe_handler() ;
void doit(int connfd);
int  legal_request(const char *method, const char *version);
void parse_uri(const char *uri, struct Url *url_data);
void build_request(char *request, const char *method, struct Url *url_data, rio_t *rio);

/* Thread_pool Part */

#define NTHREADS 10  // max number of threads
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

/* Cache Part */

// Recommended max cache and object sizes
#define MAX_CACHE_SIZE 1049000  // about 1MB
#define MAX_OBJECT_SIZE 102400
#define NCACHE_LINE 10 

typedef struct {
    int flag;           // 有效位
    int reader_cnt;     // 读取进程数
    char hdr[MAXLINE];  // HTTP响应 头部信息
    char uri[MAXLINE];
    char web_obj[MAX_OBJECT_SIZE];
    sem_t reader_cnt_mutex;
    sem_t write;
}Cache_line;
typedef struct {
    int valid_cache_lines;    // 有效的 cache 行个数
    sem_t mutex;
    Cache_line cache[NCACHE_LINE];
}Cache;
struct Node {
    int index;
    struct Node *pre;
    struct Node *next;
};
typedef struct Doublelinked_list {
    sem_t mutex;
    struct Node *head;
    struct Node *rear;
}Dbl;
// Cache functions
void init_cache(Cache *p);
int insert_cache(Cache_line *pl, Cache *p);
void update_cache(int index, Cache_line *pl, Cache *p);
int find_cache(const char *uri, Cache *p);
// LRU
void init_list(Dbl *p);
void insert_list(int index, Dbl *p);
int remove_list(Dbl *p);
void update_list(int index, Dbl *p);
void print_list(Dbl *dbl); // Debug

Pool pool;   // Creat a thread_pool
Cache cache; // Creat a cache
Dbl dbl;     // Creat a doubled_linked list

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port number>\n", argv[0]);
        return 0;
    }
    
    int listenfd, connfd;
    struct sockaddr_storage client_addr;
    socklen_t client_len; 
    char client_name[MAXLINE], client_port[MAXLINE];

    signal(SIGPIPE, sigpipe_handler);  // 捕获SIGPIPE信号，处理过早关闭的连接
    listenfd = Open_listenfd(argv[1]);
    
    init_pool(&pool);
    init_cache(&cache);
    init_list(&dbl);
    pthread_t tid;
    for (int i=0; i<NTHREADS; ++i) 
        Pthread_create(&tid, NULL, thread, NULL);
    
    while (1) {
        client_len = sizeof(client_addr);
        connfd = Accept(listenfd, (SA *)&client_addr, &client_len);
        add_client(connfd, &pool);
        Getnameinfo((SA *)&client_addr, client_len, client_name, MAXLINE, client_port, MAXLINE, 0);
        printf("Accept connection from (%s, %s)\n", client_name, client_port);
    }
    Close(listenfd);
    return 0;
}

/* Basic Part Implementation */

void sigpipe_handler() {
    fprintf(stderr, "Proxy received a sigpipe.");
    return ;
}

void doit(int connfd) {
    char buf[MAXLINE];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];  
    rio_t rio, server_rio;

    // Read request line
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); 
    
    // Check the legality of request line
    if (!legal_request(method, version)) {
        printf("Illeagl request!\n");
        return ;
    }
    
    // Parse url
    struct Url *url_data = (struct Url *)malloc(sizeof(struct Url) );
    memset(url_data, 0, sizeof(struct Url));
    parse_uri(uri, url_data);
    
    int index = find_cache(url_data->path, &cache);
    if (index == -1)  {
        // Send and receive message
        char request[MAXLINE];
        memset(request, 0, MAXLINE);
        build_request(request, method, url_data, &rio);  // 生成 HTTP 请求信息，补充url

        // Send to server
        int serverfd = Open_clientfd(url_data->host, url_data->port);
        if (serverfd < 0 ) {
            printf("Connecting to server failed.\n");
            return ;
        }
        // printf("Proxy received %d bytes, then repost to server\n", (int) strlen(request));
        Rio_writen(serverfd, request, strlen(request));  // 转发给服务器}
    
        // Send to client & 写入 Cache, 更新 LRU 
        Rio_readinitb(&server_rio, serverfd);
        size_t n;
        char hdr[MAXLINE];
        char web_obj[MAX_OBJECT_SIZE];
        int hdr_flag = 1;
        memset(hdr, 0, MAXLINE);
        memset(web_obj, 0, MAX_OBJECT_SIZE);

        while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
            // printf("proxy received %d bytes,then send\n", (int)n);
            Rio_writen(connfd, buf, n);
            if (hdr_flag) {
                if (!strcmp(buf, "\r\n")) 
                    hdr_flag = 0;
                strcat(hdr, buf);
            }
            else 
                strcat(web_obj, buf);
        }
        Close(serverfd);
        // 写入 Cache, 更新 LRU
        Cache_line temp;
        temp.flag = 1;
        temp.reader_cnt = 0;
        strcpy(temp.hdr, hdr);
        strcpy(temp.web_obj, web_obj);
        strcpy(temp.uri, url_data->path);
        sem_init(&temp.reader_cnt_mutex, 0, 1);
        sem_init(&temp.write, 0, 1);
        
        if (cache.valid_cache_lines < NCACHE_LINE) {    
            if (sizeof(temp.web_obj) <= MAX_OBJECT_SIZE) {
                // insert
                int index = insert_cache(&temp, &cache);
                // LRU
                insert_list(index, &dbl);
                print_list(&dbl);
            }
        }
        else {
            // replace 
            int index = remove_list(&dbl);
            insert_list(index, &dbl);
            // 写者:
            update_cache(index, &temp, &cache);
        }

    }
    else {
        // 读者:
        Cache_line *clp = &(cache.cache[index]);
        P(&clp->write);
        
        P(&clp->reader_cnt_mutex);
        clp->reader_cnt += 1;
        V(&clp->reader_cnt_mutex);
        
        Rio_writen(connfd, clp->hdr, strlen(clp->hdr));
        Rio_writen(connfd, clp->web_obj, strlen(clp->web_obj)); 
        
        P(&clp->reader_cnt_mutex);
        clp->reader_cnt -= 1;
        if (clp->reader_cnt == 0)
            V(&clp->write);
        V(&clp->reader_cnt_mutex);
        // 更新 LRU
        update_list(index, &dbl);
    }
}

int legal_request(const char *method, const char *version) {
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST") ) 
        return 0;
    if (strcasecmp(version, "HTTP/1.0") && strcasecmp(version, "HTTP/1.1")) 
        return 0;
    return 1;
}

void parse_uri(const char *uri, struct Url *url_data) {
    char *url_start = strstr(uri, "//");
    char *uri_start;
    // URI
    if (url_start == NULL)  
        uri_start = strstr(uri, "/");   
    // URL
    else 
        uri_start = strstr(url_start+2, "/");
    strcpy(url_data->path, uri_start);
}

void build_request(char *request, const char *method, struct Url *url_data, rio_t *rio) {
    // 生成请求行
    char request_line[MAXLINE];
    char *request_line_fmt = "%s %s HTTP/1.0\r\n";
    sprintf(request_line, request_line_fmt, method, url_data->path);

    // 读取接收的消息头信息
    char extra_hdrs[MAXLINE];
    extra_hdrs[0] = '\0';
    char buf[MAXLINE];
    while (Rio_readlineb(rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)
            break;
        else {
            char *host = strstr(buf, "Host: ");
            if (host) {
                host += 6;
                char *port = strstr(host, ":");
                if (port) {
                    port += 1;
                    char *port_end = strstr(port, "\r\n");
                    strncpy(url_data->host, host, port-1-host);
                    strncpy(url_data->port, port, port_end-port);
                }
                else { 
                    strcpy(url_data->port, "80");
                    char *host_end = strstr(port, "\r\n");
                    strncpy(url_data->host, host, host_end-host);
                }
            }
            else if (!strstr(buf, "User-Agent") && !strstr(buf, "Connection") && !strstr(buf, "Proxy-Connection")) 
                strcpy(extra_hdrs, buf);
        }
    }
    char host_hdr[MAXLINE];
    strcpy(host_hdr, "Host: ");
    strcat(host_hdr, url_data->host);
    if (strcmp(url_data->port, "80"))
        strcat(strcat(host_hdr, ":"), url_data->port);
    strcat(host_hdr, "\r\n");
    // user-agent hdr provided
    char connection_hdr[] = "Connection: close\r\n";
    char proxy_connection_hdr[] = "Proxy-Connection: close\r\n";
    
    // 生成消息: HOST、 User-Agent、 Connection、 Proxy-Connection
    sprintf(request, "%s%s%s%s%s%s\r\n",
            request_line,
            host_hdr,
            connection_hdr,
            proxy_connection_hdr,
            user_agent_hdr,
            extra_hdrs);
    return ;
}

/* Thread_pool Part Implementation */
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
            p->fd_free[i] = -1;
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

void *thread(void *vargp) {
    pthread_t self_tid = pthread_self(); 
    pthread_detach(self_tid);
    int connfd;
    connfd = get_fd(&pool);
    printf("TID:%lu working.\n", self_tid);
    doit(connfd);
    Close(connfd);
    remove_client(connfd, &pool); // 释放线程池
    return NULL;
}

/* Cache Part Implementation */

// 初始化 cache
void init_cache(Cache *p) {
    p->valid_cache_lines = 0;
    sem_init(&p->mutex, 0, 1);
    for (int i=0; i<NCACHE_LINE; ++i) {
        Cache_line *pl = &(p->cache[i]);
        pl->flag = 0;
        pl->reader_cnt = 0;
        memset(pl->hdr, 0, MAXLINE);
        memset(pl->uri, 0, MAXLINE);
        memset(pl->web_obj, 0, MAX_OBJECT_SIZE);
        sem_init(&pl->reader_cnt_mutex, 0, 1);
        sem_init(&pl->write, 0, 1);
    }
}

// 将对应的 cache_line 数据写入 cache 空行中, 返回写入的index
int insert_cache(Cache_line *clps, Cache *p) {
    P(&p->mutex);
    for (int i=0; i<NCACHE_LINE; ++i) {
        Cache_line *clpd = &(p->cache[i]); 
        if (clpd->flag == 0) {
            memcpy(clpd, clps, sizeof(Cache_line));
            p->valid_cache_lines += 1;
            V(&p->mutex);
            return i;
       }
    }
    V(&p->mutex);
    return -1;
}

// 将cache[index]的 cache_line 更新
void update_cache(int index, Cache_line *pl, Cache *p) {
    P(&p->mutex);
    Cache_line *old = &(p->cache[index]);
    P(&old->write);
    memcpy(old, pl, sizeof(Cache_line));
    V(&old->write);
    V(&p->mutex);
}

// 根据 URI 在 cache 中查找cache_line, 返回索引
int find_cache(const char *uri, Cache *p) {
    P(&p->mutex);
    for (int i=0; i<NCACHE_LINE; ++i) {
       Cache_line *pl = &(p->cache[i]);
       if ( !strcasecmp(pl->uri, uri) && pl->flag == 1) {
            V(&p->mutex);
            return i;
       }
    }
    V(&p->mutex);
    return -1;
}

/* LRU */

void init_list(Dbl *p) {
    struct Node *head = malloc(sizeof(struct Node));
    struct Node *rear = malloc(sizeof(struct Node));
    head->index = -1;
    head->pre = NULL;
    head->next = rear;
    rear->index = -1;
    rear->pre = head;
    rear->next = NULL;
    
    p->head = head;
    p->rear = rear;
    sem_init(&p->mutex, 0, 1);
}

void insert_list(int index, Dbl *p) {
    // 始终插入尾部: Cache未满时
    struct Node *np = (struct Node *)malloc(sizeof(struct Node));
    np->index = index;
    P(&p->mutex);
    struct Node *last = p->rear->pre;
    last->next = np;
    np->pre = last;
    np->next = p->rear;
    p->rear->pre = np;
    V(&p->mutex);
}

int remove_list(Dbl *p) {
    // 始终删除头部: Cache满时
    P(&p->mutex);
    struct Node *del = p->head->next;
    p->head->next = del->next;
    del->next->pre = p->head;
    V(&p->mutex);
    int index = del->index;
    free(del);
    return index;
}

void update_list(int index, Dbl *p) {
    if (index < 0 || index >= NCACHE_LINE) 
        app_error("update_list error : no such index!");
    
    P(&p->mutex);
    struct Node *np = p->head->next;
    while (np != p->rear) {
        if (np->index == index) {
            np->pre->next = np->next;
            np->next->pre = np->pre;
            V(&p->mutex);
            free(np);
            insert_list(index, p);
            print_list(p);  // debug
            return ; 
        }
        np = np->next;
    }
    V(&p->mutex);
    print_list(p);  // debug
    app_error("update_list error : no such index!");
}
// Debug
void print_list(Dbl *dbl) {
    struct Node *p = dbl->head;
    while (p) {
        if (p != dbl->rear)
            printf("%d <-> ", p->index);
        else
            printf("%d", p->index);
        p = p->next;
    }
    putchar('\n');
}