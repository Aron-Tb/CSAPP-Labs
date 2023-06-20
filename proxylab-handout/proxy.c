#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

struct Url_part {
    char host[MAXLINE];
    char port[MAXLINE];
    char uri[MAXLINE];
};

// Some function's  prototypes needed
void sigpipe_handler() ;
void doit(int connfd);
int legalRequest(const char *method, const char *url, const char *version);
void parse_url(const char *url, struct Url_part *url_data);
void build_request(char *request, const char *method, struct Url_part *url_data, rio_t *rio);

int main(int argc, char **argv)
{
    // 1. 等待客户端连接
    // 2. 连接建立
    // 3. 输出连接信息
    // 4. 解析和处理请求

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port number>\n", argv[0]);
        return 0;
    }
    
    char client_name[MAXLINE], client_port[MAXLINE];
    int listenfd, connfd;
    struct sockaddr_storage client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    signal(SIGPIPE, sigpipe_handler);  // 捕获SIGPIPE信号，处理过早关闭的连接
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        connfd = Accept(listenfd, (SA *)&client_addr, &client_len);
        Getnameinfo((SA *)&client_addr, client_len, client_name, MAXLINE, client_port, MAXLINE, 0);
        printf("Accept connection from (%s, %s)\n", client_name, client_port);
        doit(connfd);
        Close(connfd);
    }
    Close(listenfd);
    printf("%s", user_agent_hdr);
    return 0;
}

void sigpipe_handler() {
    fprintf(stderr, "Proxy received a sigpipe.");
    return ;
}

void doit(int connfd) {
    char buf[MAXLINE];
    char method[MAXLINE], url[MAXLINE], version[MAXLINE];  // 规定了客户端发送 URL    
    rio_t rio, server_rio;

    // Read request line
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, url, version); 
    
    // Check the legality of request line
    if (!legalRequest(method, url, version)) {
        printf("Illeagl request!\n");
        return ;
    }
    
    // Parse url
    struct Url_part *url_data = (struct Url_part *)malloc(sizeof(struct Url_part) );
    parse_url(url, url_data);

    // Send and receive message
    char request[MAXLINE];
    memset(request, 0, MAXLINE);
    
    build_request(request, method, url_data, &rio);
    
    // Send to server
    int serverfd = Open_clientfd(url_data->host, url_data->port);
    if (serverfd < 0 ) {
        printf("Connecting to server failed.\n");
        return ;
    }
    printf("Proxy received %d bytes, the repost to server\n", (int) strlen(request));
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

int legalRequest(const char *method, const char *url, const char *version) {
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST") ) 
        return 0;
    if (strcasecmp(version, "HTTP/1.0") && strcasecmp(version, "HTTP/1.1"))
        return 0;
    if (!strstr(url, "//"))
        return 0;
    return 1;
}

void parse_url(const char *url, struct Url_part *url_data) {
    char *host_start = strstr(url, "//") + 2;
    char *host_end = strstr(host_start, "/") - 1;
    
    memcpy(url_data->host, host_start, host_end - host_start + 1);
    url_data->host[host_end - host_start + 1] = '\0';
    
    char *port_start = strstr(url, ":");
    char *uri_start;
    if (port_start == NULL) {
        strcpy(url_data->port, "80");
        uri_start = host_end + 1;
    }
    else {
        port_start += 1;
        char *port_end = strstr(port_start, "/") - 1;
        memcpy(url_data->port, port_start, port_end - port_start + 1);
        url_data->port[port_end - port_start + 1] = '\0';
        uri_start = port_end + 1;
    }
    
    strcpy(url_data->uri, uri_start);
}

void build_request(char *request, const char *method, struct Url_part *url_data, rio_t *rio) {
    char request_line[MAXLINE];
    char *request_line_fmt = "%s %s HTTP/1.0\r\n";
    sprintf(request_line, request_line_fmt, method, url_data->uri);

    char host_hdr[MAXLINE];
    char *host_hdr_fmt = "Host: %s\r\n";
    sprintf(host_hdr, host_hdr_fmt, url_data->host);

    // user-agent hdr provided
    char connection_hdr[] = "Connection: close\r\n";
    char proxy_connection_hdr[] = "Proxy-Connection: close\r\n";

    // 读取消息头其他信息
    char extra_hdrs[MAXLINE];
    extra_hdrs[0] = '\0';
    char buf[MAXLINE];
    while (Rio_readlineb(rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)
            break;
        if (!strstr(buf, "Host") &&  !strstr(buf, "Connection: ") && !strstr(buf, "Proxy-Connection: ") && !strstr(buf, user_agent_hdr))
            strcat(extra_hdrs, buf);
    }
    sprintf(request, "%s%s%s%s%s%s\r\n",
            request_line,
            host_hdr,
            connection_hdr,
            proxy_connection_hdr,
            user_agent_hdr,
            extra_hdrs);
       
    // 读取消息体
    while (Rio_readlineb(rio, buf, MAXLINE) > 0) 
        strcat(request, buf);
    
    return ;
}