#include "csapp.h"
#include "thread_pool.h"
#include "cache.h"

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

struct Url {
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
};

/* Some function's  prototypes needed */
void sigpipe_handler() ;
void doit(int connfd);
int  legal_request(const char *method, const char *version);
void parse_uri(const char *uri, struct Url *url_data);
void build_request(char *request, const char *method, struct Url *url_data, rio_t *rio);

// Creat a thread_pool
Pool pool;
// Creat a cache
Cache cache;
// Creat a doubled_linked list
Dbl dbl;

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
    init_cache(&cache);
    init_list(&dbl);

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
        printf("Proxy received %d bytes, then repost to server\n", (int) strlen(request));
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
            printf("proxy received %d bytes,then send\n", (int)n);
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
            // insert
            int index = insert_cache(&temp, &cache);
            // LRU
            insert_list(index, &dbl);
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