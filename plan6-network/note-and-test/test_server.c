#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>

#define SERVE_PORT 9527

void sys_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

char buf[BUFSIZ];

int main() {
    int lfd = 0, cfd = 0, ret;
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        sys_err("socket");
    }
    struct sockaddr_in serv_addr, clit_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVE_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(lfd, 12);
    socklen_t clit_addr_len = sizeof(clit_addr);
    cfd = accept(lfd, (struct sockaddr *)&clit_addr, &clit_addr_len);
    if (cfd == -1) {
        sys_err("accept");
    }
    while (1) {
        ret = read(cfd, buf, sizeof(buf));
        printf("Received: %s", buf);
        for (int i = 0; i < ret; ++i) {
            buf[i] = toupper(buf[i]);
        }
        write(cfd, buf, sizeof(buf));
        sleep(1);
    }
    close(lfd);
    close(cfd);
}