#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include "wrap.h"

#define MAXLINE 80
#define SERV_PORT 9000

char buf[BUFSIZ];

int main() {
    int lfd = 0, cfd = 0, ret;
    struct sockaddr_in serv_addr, clit_addr;

    pthread_t tid;


    return 0;
}