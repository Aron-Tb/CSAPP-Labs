#include <unistd.h>
#include <errno.h>

#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;                /* Descriptor for this internal buf */
    int rio_cnt;               /* Unread bytes in internal buf */
    char *rio_bufptr;          /* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
}rio_t;

void rio_readinitb(rio_t *rp, int fd) {
    rp->rio_fd = fd;    
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

ssize_t rio_read(rio_t rp, char *usrbuf, size_t n) {

}





ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t max_len) {
    size_t nread = 0;
    char *bp = usrbuf;
    while (*(rp->rio_bufptr) != '\n' && nread < max_len) {
        *bp = *(rp->rio_bufptr);
        ++nread;
        ++bp;
    }
    return nread;
}


ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);


