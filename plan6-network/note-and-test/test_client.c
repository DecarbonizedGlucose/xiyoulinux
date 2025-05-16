#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>

#define SERV_PORT 9527

char buf[BUFSIZ];

void sys_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd == -1) sys_err("socket");
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
    int ret = connect(cfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (ret == -1) sys_err("connect");
    int cnt = 10;
    while (cnt--) {
        printf("> ");
        fgets(buf, sizeof(buf), stdin);
        write(cfd, buf, sizeof(buf));
        sleep(1);
        read(cfd, buf, sizeof(buf));
        printf("%s", buf);
    }
    close(cfd);
    putchar('\n');
    return 0;
}