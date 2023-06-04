#include <unistd.h>
#include <errno.h>
#include <string.h>

/* 
 * RIO functions without buffer.
 */
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

/* 
 * RIO functions With buffer.
 */

#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;                /* Descriptor for this internal buf */
    int rio_cnt;               /* Unread bytes in internal buf */
    char *rio_bufptr;          /* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
}rio_t;

void rio_readinitb(rio_t *rp, int fd) {
// 将fd与缓冲区联系起来
    rp->rio_fd = fd;    
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n) {
    while (rp->rio_cnt <= 0) {            // nothing to read, call read function
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));  // read放入缓冲区，均为未读字节
        if (rp->rio_cnt < 0) {  
            if (errno == EINTR)           // Interupted 
                continue;
            else
                return -1;                // Error
        }
        else if (rp->rio_cnt == 0 )       // EOF
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf; // reset pointer
    }
    
    /* copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    int cnt = rp->rio_cnt;
    if (n < cnt)
        cnt = n;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t max_len) {
    int cnt, rc;
    char c, *bufp = usrbuf;
    for (cnt = 1; cnt < max_len; ++cnt) {
        rc = rio_read(rp, &c, 1);
        /* rc < 0 */
        if (rc < 0) 
            return -1;     // Error
        /* rc == 0 */
        else if (rc == 0)  // EOF
            break;
        /* rc == 1 */
        else if (rc == 1) {
            *bufp++ = c;
            if (c == '\n') {
                ++cnt;
                break;
            }
        }
    }
    *bufp = 0;  // \0
    return cnt - 1;
}

ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) {
    ssize_t nread = 0;
    size_t nleft = n;
    char *bufp = usrbuf;
    while (nleft > 0) {
        ssize_t cur_read = rio_read(rp, bufp, nleft);
        if (cur_read == 0)  // EOF
            break;
        else if (cur_read == -1)  // Error
            return -1;
        else {
            nleft -= cur_read;
            nread += cur_read;
            bufp += cur_read;
        }
    }
    return nread;
}