/*
    select server，但是添加了自定义fd数组
    array-select-server.c
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
    int i, j, n, nready, maxi;
    int maxfd = 0, lfd, cfd, sockfd;
    int client[FD_SETSIZE];                       /* 其实就是1024 */
    char buf[BUFSIZ], str[INET_ADDRSTRLEN];       /* BUFSIZ, 8192; 网络地址字符串长度, 16 */
    memset(client, -1, sizeof(client));

    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    lfd = Socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_PORT);
    Bind(lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    Listen(lfd, 128);

    fd_set rset, allset;
    FD_ZERO(&allset);
    FD_SET(lfd, &allset);
    maxfd = lfd; // 初始の最大文件描述符

    while (1) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            perr_exit("select error");
        }

        if (FD_ISSET(lfd, &rset)) {
            clnt_addr_len = sizeof(clnt_addr);
            cfd = Accept(lfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len); /* Accept不阻塞 */

            /* ----- new codes ----- */
            for (i = 0; i < FD_SETSIZE; ++i) {
                if (client[i] < 0) {
                    client[i] = cfd;              /* 添加到自己的数组中 */
                    break;
                }
            }
            if (i == FD_SETSIZE) { // i == 1024, 也就是没有空位了
                perr_exit("too many clients");
            }
            maxi = (i > maxi) ? i : maxi;
            /* --------------------- */

            FD_SET(cfd, &allset);
            maxfd = (cfd > maxfd) ? cfd : maxfd;

            if (nready == 1) {
                continue;
            }
        }

        // 处理已有的连接
        for (i = 0; i <= maxi; ++i) {       /* 这样遍历减少不必要的判断 */
            if ((sockfd = client[i]) < 0 ) {
                continue;
            } // 取出fd, -1是关闭的
            if (FD_ISSET(sockfd, &rset)) {
                n = Read(sockfd, buf, sizeof(buf));
                if (n == 0) {
                    Close(sockfd);
                    printf("Client closed.\n");
                    FD_CLR(sockfd, &allset); // 除名
                    client[i] = -1; // 关闭后，设置为-1
                    continue;
                }
                printf("Received: %s\n", buf);
                for (j = 0; j < n; ++j) {
                    buf[j] = toupper(buf[j]);
                }
                Write(sockfd, buf, n);
            }
            if (nready == 1) {
                break; // 当前活跃的fd已经处理完了
            }
        }
    }

    Close(lfd);
    // 关闭所有连接
    for (i = 0; i <= maxi; ++i) {
        if (FD_ISSET(client[i], &allset)) {
            Close(client[i]);
            client[i] = -1; // 关闭后，设置为-1
        }
    }

    return 0;
}