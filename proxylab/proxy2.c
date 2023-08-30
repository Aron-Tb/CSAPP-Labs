#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

struct CacheLine{
    char content[MAX_OBJECT_SIZE];
};

typedef struct {
    char *cache[MAX_CACHE_SIZE];
}Cache;



/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

// some structs
struct Url {
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
};

/* Threads pool */
#define NTHREADS 100  // max number of threads
typedef struct {
    sem_t mutex; 
    sem_t slots;     // 空闲槽数量
    sem_t free_fds;  // 需要被线程服务的连接数
    int fd_set[NTHREADS];
    int fd_free[NTHREADS];  // 标记连接是否正在被服务: 0正在被服务、1未被服务、-1不存在连接(fd_set = -1)
}Pool;
static Pool pool;

// Some function's  prototypes needed
void sigpipe_handler() ;
void doit(int connfd);
int  legal_request(const char *method, const char *version);
void parse_uri(const char *uri, struct Url *url_data);
void build_request(char *request, const char *method, struct Url *url_data, rio_t *rio);
// Thread_pool functions
void init_pool(Pool *p);
void add_client(int connfd, Pool *p);
void remove_client(int connfd, Pool *p);
void *thread(void *vargp);

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
    
    pthread_t tid;
    for (int i=0; i<NTHREADS; ++i) 
        Pthread_create(&tid, NULL, thread, NULL);
    init_pool(&pool);
    
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

    // Send and receive message
    char request[MAXLINE];
    memset(request, 0, MAXLINE);
    build_request(request, method, url_data, &rio);  // 生成 HTTP 请求信息，补充url

    //printf("Request:\n%s", request);

    // Send to server
    int serverfd = Open_clientfd(url_data->host, url_data->port);
    if (serverfd < 0 ) {
        printf("Connecting to server failed.\n");
        return ;
    }
    printf("Proxy received %d bytes, then repost to server\n", (int) strlen(request));
    Rio_writen(serverfd, request, strlen(request));  // 转发给服务器
    
    // Send to client
    Rio_readinitb(&server_rio, serverfd);
    size_t n;
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0)
    {
        printf("proxy received %d bytes,then send\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
    Close(serverfd);
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