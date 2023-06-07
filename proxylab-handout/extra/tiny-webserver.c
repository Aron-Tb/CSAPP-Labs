#include "csapp.h"

void doit(int fd);
void read_requestdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char hostname[MAXLINE], client_port[MAXLINE];
    int listenfd, connfd;
    struct sockaddr_storage client_addr;
    socklen_t clientlen = sizeof(client_addr);
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        connfd = Accept(listenfd, (SA *)&client_addr, &clientlen);
        Getnameinfo((SA *)&client_addr, clientlen, hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Accept connection from (%s, %s)\n", hostname, client_port);
        doit(connfd);
        Close(connfd);
    }
    Close(listenfd);
    return 0;
}

void doit(int connfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    struct stat file_matadata;
    int is_static;
    rio_t rio;
    
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);  // Read request line
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(connfd, method, "501", "Not implemented", "Tiny does not implement this method");
        return ;
    }
    read_requestdrs(&rio); // ?

    is_static = parse_uri(uri, filename, cgiargs);
    
    if (stat(filename, &file_matadata) < 0 ) {
        clienterror(connfd, filename, "404", "Not found", "Tiny couldn't find the file");
        return ;
    }

    if (is_static) {
        if ( S_ISREG(file_matadata.st_mode) || !(S_IRUSR & file_matadata.st_mode) ) {
            clienterror(connfd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return ;
        }
        serve_static(connfd, filename, file_matadata.st_size);
    }

    else {
        if (!(S_ISREG(file_matadata.st_mode)) || !(S_IXUSR & file_matadata.st_mode) ) {
            clienterror(connfd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return ;
        }
        serve_dynamic(connfd, filename, cgiargs);
    }
}

/* 辅助函数 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];
    /* 构造HTTP响应信息 */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n, body, longmsg, cause");
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* 打印HTTP响应信息 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_requestdrs(rio_t *rp) {
// 读取并忽略请求报头
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return ;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
// 假设静态内容的主目录是当前目录，可执行目录的主目录是./cgi-bin
    int is_static;
    char *ptr;
    if (!strstr(uri, "cgi-bin")) {
        is_static = 1;
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/')
            strcat(filename, "home.html");
    }
    else {
        is_static = 0;
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else 
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
    }
    return is_static;
}

void serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);    
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf)); 

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); 
    srcp = Mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  // 文件映射到虚拟内存空间
    Close(srcfd);                       
    Rio_writen(fd, srcp, filesize);     
    Munmap(srcp, filesize);             
}

void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
	    strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	    strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	    strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	    strcpy(filetype, "image/jpeg");
    else
	    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* Child */ 
	/* Real server would set all CGI vars here */
	    setenv("QUERY_STRING", cgiargs, 1); 
	    Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ 
	    Execve(filename, emptylist, environ); /* Run CGI program */ 
    }
    Wait(NULL); /* Parent waits for and reaps child */ 
}