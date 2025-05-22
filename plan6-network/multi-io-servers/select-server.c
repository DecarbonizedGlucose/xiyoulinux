/*
    简单的多路复用IO服务器(select)
    select-server.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <ctype.h>
#include <unistd.h>
#include "wrap.h"

#define SERV_PORT 9000

int main() {
    int i, j, n, nready;
    int maxfd = 0; // 最大文件描述符
    int listenfd, connfd;
    char buf[BUFSIZ];

    /* ----- socket连接公式化代码 ----- */
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // 端口复用
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_PORT);
    Bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    Listen(listenfd, 128);
    /* --------------------------------- */

    fd_set rset, allset;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd; // 初始の最大文件描述符

    while (1) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            perr_exit("select error");
        }

        /*
            从代码逻辑来看，select起到了“需时取用”的作用。
            有事件发生时，select再让server处理，避免了server一直在while循环中等待。
        */

        // listen还在rset里吗？
        // 如果在，就是有新的连接
        if (FD_ISSET(listenfd, &rset)) {
            clnt_addr_len = sizeof(clnt_addr);
            connfd = Accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len); /* Accept不阻塞 */

            FD_SET(connfd, &allset); // 将新连接加入到集合中
            maxfd = (connfd > maxfd) ? connfd : maxfd; // 实时更新最大文件描述符

            if (nready == 1) { /* 只有listenfd有事件，没有新连接，继续等待。后续代码不执行 */
                continue;
            }
        }

        // 处理已有的连接
        for (i = listenfd + 1; i <= maxfd; ++i) {
            if (FD_ISSET(i, &rset)) {
                n = Read(i, buf, sizeof(buf));
                if (n == 0) { // 到这跟之前写的一样了
                    // 连接关闭
                    Close(i);
                    printf("Client closed.\n");
                    FD_CLR(i, &allset); // 除名
                    continue;
                }
                printf("Received: %s\n", buf);
                for (j = 0; j < n; ++j) {
                    buf[j] = toupper(buf[j]);
                }
                Write(i, buf, n); // 发送给客户端
            }
        }
    }

    Close(listenfd);
    // 关闭所有连接
    for (i = listenfd + 1; i <= maxfd; ++i) {
        if (FD_ISSET(i, &allset)) {
            Close(i);
        }
    }

    return 0;
}