#include "csapp.h"

int open_clientfd(char *hostname, char *port) {
// 返回一个已连接的套接字描述符
    int clientfd;
    struct addrinfo *p, *listp, hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;

    Getaddrinfo(hostname, port, &hints, &listp);
    for (p = listp; p != NULL; p = p->ai_next) {
        clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (clientfd == -1 )
            continue;
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break;
        Close(clientfd);
    }
    Freeaddrinfo(listp);
    if (!p)
        return -1;
    else
        return clientfd;
}

int open_listenfd(char *port) {
    int listenfd, optval = 1;
    struct addrinfo *p, *listp, hints;
    memset(&hints, 0, sizeof(hints));
    
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
    Getaddrinfo(NULL, port, &hints, &listp);

    for (p = listp; p != NULL; p = p->ai_next) {
        listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listenfd == -1)
            continue;
        
        // Setsockopt
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
        
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break;
        Close(listenfd);
    }
    
    Freeaddrinfo(listp);
    if (!p)
        return -1;
    
    if (listen(listenfd, LISTENQ) == -1) {
        Close(listenfd);
        return -1;
    }
    return listenfd;
}