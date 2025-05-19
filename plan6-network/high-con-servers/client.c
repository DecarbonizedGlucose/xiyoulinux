#include <stdio.h>
#include "wrap.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERV_PORT 9000

char buf[BUFSIZ];

int main(int argc, char* argv[]) {
    int cfd = Socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
    int ret = Connect(cfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    while (1) {
        printf("> ");
        fgets(buf, sizeof(buf), stdin);
        Write(cfd, buf, sizeof(buf));
        sleep(1);
        Read(cfd, buf, sizeof(buf));
        printf("%s", buf);
    }
    Close(cfd);
    putchar('\n');
    return 0;
}