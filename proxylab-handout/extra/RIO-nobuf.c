#include <unistd.h>
#include <errno.h>

ssize_t rio_readn(int fd, void *userbuf, size_t n) {
    ssize_t nread = 0;
    size_t nleft = n;
    char *bp = userbuf;
    while ( nleft > 0) {
        if ((nread = read(fd, bp, nleft)) < 0) {
            if (errno == EINTR) 
                nread = 0;
            else
                return -1;
        }
        else if (nread == 0)
            break;
        
        nleft -= nread;
        bp += nread;
        return n - nleft;
    }
}

ssize_t rio_writen(int fd, void *userbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten = 0;
    char *bp = userbuf;
    while (nleft > 0) {
        if ((nwritten = write(fd, bp, nleft)) < 0) {
            if (errno == EINTR)
                nwritten = 0;
            else
                return -1;
        }
        nleft -= nwritten;
        bp += nwritten;
    }
    return n - nleft;
}