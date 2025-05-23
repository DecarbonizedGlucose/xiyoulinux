/*
    epoll反应堆模型
    epoll-reactor-model.c
    边缘触发
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
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "wrap.h"

#define MAX_EVENTS 10
#define SERV_PORT 9000
#define MAX_CLNT 128
#define OPEN_MAX 1024

void recvdata(int fd, int events, void* arg) {
    struct myevent_s* pmye = (struct myevent_s*)arg;
    int len = Read(fd, pmye->buf, sizeof(pmye->buf));

}

void senddata(int fd, int events, void* arg) {
    struct myevent_s* pmye = (struct myevent_s*)arg;
    
}

struct myevent_s {
    int    fd;                                           /* 要监听的fd */
    int    events;                                       /* 监听事件 */
    void*  arg;                                          /* 泛型参数 */
    void   (*call_back)(int fd, int events, void* arg);  /* 事件回调函数 */
    int    status;                                       /* 事件监听状态 1: 在树上 0: 不在*/
    char   buf[BUFSIZ];
    int    buflen;
    long   last_active;                                  /* 上次加入红黑树的时间值 */
};

void set_event(
    struct myevent_s* ev,
    int fd,
    void (*call_back)(int, int, void*),
    void* arg
) {
    ev->fd = fd;
    ev->call_back = call_back;
    ev->events = 0;
    ev->arg = arg;
    ev->status = 0;
    memset(ev->buf, 0, sizeof(ev->buf));
    ev->buflen = 0;
    ev->last_active = time(NULL);
}

void add_event(int efd, int events, struct myevent_s* myev) {
    struct epoll_event ev = {0, {0}};
    int op = 0;
    ev.data.ptr = myev;
    ev.events = myev->events = events;
    if (myev->status == 0) { // 说明是新增的事件
        op = EPOLL_CTL_ADD;
        myev->status = 1;
    } else {
        op = EPOLL_CTL_MOD;
    }
    if (epoll_ctl(efd, op, myev->fd, &ev) < 0) {
        printf("Failed to add event: epoll_ctl error: %s\n", strerror(errno));
    }
    else {
        printf("Successfully added event: fd=%d, events=%d\n", myev->fd, events);
    }
    return;
}

void del_event(int efd, struct myevent_s* myev) {
    if (myev->status != 1) {
        return;
    }
    myev->status = 0;
    epoll_ctl(efd, EPOLL_CTL_DEL, myev->fd, NULL);
    printf("Deleted event: fd=%d\n", myev->fd);
    Close(myev->fd);
    /*
    myev->fd = -1;
    myev->events = 0;
    myev->call_back = NULL;
    myev->arg = NULL;
    myev->buflen = 0;
    memset(myev->buf, 0, sizeof(myev->buf));
    myev->last_active = 0;
    */
    return;
}

int g_efd;
struct myevent_s* g_events[MAX_EVENTS + 1]; // 事件数组, 最后一个留给lfd

void acceptconn(int lfd, int events, void* arg) { // 把新来的连接加入到红黑树上
    struct sockaddr_in clnt_addr;
    int i;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    int cfd = Accept(lfd, (struct sockaddr *)&clnt_addr, &clnt_addr_len);
    if (cfd == -1) {
        perr_exit("Accept");
    }
    struct myevent_s* pmye;
    do {
        for (i=0; i<MAX_EVENTS; ++i) {
            if (g_events[i]->status == 0) {
                pmye = g_events[i];
                break;
            }
        }
        if (i == MAX_EVENTS) {
            printf("%s: too many clients\n", __func__);
            Close(cfd);
            return;
        }
        int flag = fcntl(cfd, F_SETFL, O_NONBLOCK); // 设置非阻塞
        if (flag == -1) {
            printf("%s: failed to set non-blocking: %s\n", __func__, strerror(errno));
        }
        set_event(pyme, cfd, recvdata, pmye);
        add_event(g_efd, EPOLLIN | EPOLLET, pmye);
    } while (0);
}

void initlistensocket(int efd, short port) {
    int lfd;
    struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    lfd = Socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    Bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    Listen(lfd, MAX_CLNT);
    set_event(&g_events[MAX_EVENTS], lfd, acceptconn, &g_events[MAX_EVENTS]);
    add_event(g_efd, EPOLLIN, &g_events[MAX_EVENTS]);
}

int main() {
    int ret, nready;
    struct epoll_event ep[OPEN_MAX]; // epoll相关

    g_efd = epoll_create(OPEN_MAX);
    if (g_efd == -1) {
        perr_exit("epoll_create");
    }

    initlistensocket(g_efd, SERV_PORT);
    if (ret == -1) {
        perr_exit("epoll_ctl");
    }

    while (1) {
        nready = epoll_wait(g_efd, ep, OPEN_MAX, 1000); // 阻塞等待
        if (nready == -1 && errno != EINTR) {
            perr_exit("epoll_wait");
        }
        else if (nready == 0) {
            continue; // 超时
        }
        else {
            // 有事件发生
            for (int i=0; i<nready; ++i) {
                if (ep[i].data.fd == lfd) {
                    acceptconn(lfd, ep[i].events, &g_events[MAX_EVENTS]);
                }
                else {
                    struct myevent_t* ev = ep[i].data.ptr;
                    if ((ep[i].events & EPOLLIN) && (ev.events & EPOLLIN)) {
                        ev->call_back(ev->fd, ep[i].events, ev->arg);
                    }
                    if ((ep[i].events & EPOLLOUT) && (ev.events & EPOLLOUT)) {
                        ev->call_back(ev->fd, ep[i].events, ev->arg);
                    }
                }
            }
        }
    }

    Close(lfd);
    CLose(g_efd);
    return 0;
}