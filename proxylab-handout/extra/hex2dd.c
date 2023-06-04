#include <arpa/inet.h>
#include <stdio.h>

#define BUFSIZE 16

int main(int argc, const char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <hex number>\n", "hex2dd");
        return -1;
    }
    // else
    char res[BUFSIZE];
    res[BUFSIZE-1] = '\0';
    struct in_addr n_ip;
    uint32_t h_ip;
    sscanf(argv[1], "%x", &h_ip);
    n_ip.s_addr = htonl(h_ip);
    
    const char *str = inet_ntop(AF_INET, &n_ip, res, BUFSIZE-1);
    if (!str) {
       fprintf(stderr, "Something goes wrong!\n"); 
       return -1;
    }
    // else
    printf("%s\n", str);
    return 0;
}