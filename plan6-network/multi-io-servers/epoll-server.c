/*
    使用epoll实现的多路IO复用服务器
    epoll-server.c
    默认水平触发
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <ctype.h>
#include "wrap.h"

#define MAX_EVENTS 10
#define SERV_PORT 9000
#define MAX_CLNT 128
#define OPEN_MAX 1024

char buf[BUFSIZ];

int main() {
    int lfd, cfd, efd, ret, nready;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_len;
    struct epoll_event tep, ep[OPEN_MAX]; // epoll相关

    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    lfd = Socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    Bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    Listen(lfd, MAX_CLNT);

    /* ----- new codes ----- */
    efd = epoll_create(OPEN_MAX);
    if (efd == -1) {
        perr_exit("epoll_create");
    }
    tep.events = EPOLLIN;
    tep.data.fd = lfd;
    ret = epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &tep); // 把lfd挂到树上
    if (ret == -1) {
        perr_exit("epoll_ctl");
    }
    /* ---------------------- */

    while (1) {
        nready = epoll_wait(efd, ep, OPEN_MAX, -1); // 阻塞等待
        if (nready == -1) {
            perr_exit("epoll_wait");
        }

        for (int i = 0; i < nready; ++i) {
            if (!(ep[i].events & EPOLLIN)) { // 代码健壮性操作
                continue; // 不是读事件, 直接跳过
            }
            if (ep[i].data.fd == lfd) { // 有新连接
                clnt_addr_len = sizeof(clnt_addr);
                cfd = Accept(lfd, (struct sockaddr *)&clnt_addr, &clnt_addr_len);
                tep.events = EPOLLIN;
                tep.data.fd = cfd; // 读事件
                ret = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &tep); // 把cfd挂到树上
                if (ret == -1) {
                    perr_exit("epoll_ctl");
                }
                printf("New client: %s:%d\n", inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
            }
            else {
                cfd = ep[i].data.fd; // 读事件
                ret = Read(cfd, buf, sizeof(buf));
                if (ret == 0) { // 客户端关闭连接
                    printf("client close: %d\n", cfd);
                    ret = epoll_ctl(efd, EPOLL_CTL_DEL, cfd, NULL); // 从树上删除
                    if (ret == -1) {
                        perr_exit("epoll_ctl");
                    }
                    Close(cfd);
                }
                else if (ret > 0) { // 读到数据
                    printf("Receive: %.*s\n", ret, buf);
                    for (int j = 0; j < ret; ++j) {
                        buf[j] = toupper(buf[j]); // 转换为大写
                    }
                    Write(cfd, buf, ret); // 回写数据
                }
                else {
                    // 读异常, 直接给他关掉
                    printf("client error: %d\n", cfd);
                    ret = epoll_ctl(efd, EPOLL_CTL_DEL, cfd, NULL); // 从树上删除
                    if (ret == -1) {
                        perr_exit("epoll_ctl");
                    }
                    Close(cfd);
                }
            }
        }
    }

    Close(lfd);
    Close(efd);
    // Close(cfd); // cfd在epoll树上, 不能直接关闭
    // epoll树上会自动关闭
    return 0;
}