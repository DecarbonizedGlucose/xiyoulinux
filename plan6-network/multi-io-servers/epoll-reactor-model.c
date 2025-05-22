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
#include "wrap.h"

#define MAX_EVENTS 10
#define SERV_PORT 9000
#define MAX_CLNT 128
#define OPEN_MAX 1024

void recvdata(int fd, int events, void* arg);
void senddata(int fd, int events, void* arg);

struct myevent_s {
    int    fd;                                           /* 要监听的fd */
    int    enevts;                                       /* 监听事件 */
    void*  arg;                                          /* 泛型参数 */
    void   (*call_back)(int fd, int events, void* arg);  /* 事件回调函数 */
    int    status;                                       /* 事件监听状态 1: 在树上 0: 不在*/
    char   buf[BUFSIZ];
    int    buflen;
    long   last_active;                                  /* 上次加入红黑树的时间值 */
};

int global_efd;
struct myevent_s* g_events[MAX_EVENTS + 1]; // 事件数组