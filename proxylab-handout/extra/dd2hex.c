#include <arpa/inet.h>
#include <stdio.h>

int main(int argc, const char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <ip address>\n", "dd2hex");
        return -1;
    }
    struct in_addr n_ip;
    int ok = inet_pton(AF_INET,argv[1], &n_ip);
    if (ok == 0) {
        fprintf(stderr, "Illegal Input!\n");
        return -1;
    } 
    else if (ok == -1) {
        fprintf(stderr, "Something goes wrong!\n");
        return -1;
    }
    else {
        uint32_t h_ip = ntohl(n_ip.s_addr);
        printf("0x%x\n", h_ip);
    }
    return 0;
}