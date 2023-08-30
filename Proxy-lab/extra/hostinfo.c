#include "csapp.h"
#include <string.h>


int main(int argc, const char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <hostname> \n", "hostinfo");
        exit(EXIT_FAILURE);
    }
    struct addrinfo *p, *listp, hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    Getaddrinfo(argv[1], NULL, &hints, &listp);
    
    int flag = NI_NUMERICHOST;
    char ip[MAXLINE];
    for (p = listp; p!=NULL; p=p->ai_next) {
        Getnameinfo(p->ai_addr, p->ai_addrlen, ip, MAXLINE, NULL, 0, flag);
        printf("canon-name: %s, ip address: %s \n", p->ai_canonname, ip);
    }

    Freeaddrinfo(listp);
    return 0;
}