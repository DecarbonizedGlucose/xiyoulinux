# 响应式-I/O多路复用
**select** / **poll** / **epoll** 多路io转接

# select
麻烦。<br />

### select函数
```c
/* POSIX.1-2001 */
#include <sys/select.h>
/* earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
int select(int nfds, fd_set* readfds, fd_set* writefds,
           fd_set* execptfds, struct timeval* timeout);
```
nfd代表所有被监听的fd最大值+1,是循环上限: for `[1, nfd)`，所以是循环次数+1.<br />
三个fd_set*都是传入传出参数<br />
传入多个，返回可能没那么多，因为只返回检测到有反应的<br />
readfds只检测其读事件，其余同理<br />
timeval{秒,毫秒}，参照manpage即可<br />
timeout:<br />
- NULL  表示阻塞<br />
- \> 0   监听超时时长<br />
- 0     立即返回<br />
返回值：<br />
- 成功：返回检测到的fd数量<br />
- 失败：返回-1，errno设置为EINTR或EBADF<br />
- 超时：返回0<br />

fd_set本质是位图(Bitmap)，所以提供了操作函数：
```c
void FD_CLR(int fd, fd_set* set); // 移除一个fd
int FD_ISSET(int fd, fd_set* set); // 判断
void FD_SET(int fd, fd_set* set); // 添加一个fd
void FD_ZERO(fd_set* set); // 清空
```
<br />

### select思路分析
见simple-select-server.c<br />

### select缺点
- fd_set受文件描述符限制，fd_set的大小是1024字节，最多只能监听1024个fd
- 线性扫描，效率低下
- 设计比较早，编码麻烦
我只想监听特定的几个fd，对不起，做不到🤣<br />
得自己加业务逻辑，增加代码难度<br />

### select优点
- 效率高
- 唯一一个跨平台的多路io复用接口

### select提高：增加自己的数组
见array-select-server.c<br />

# poll
比较鸡肋，跟个半成品似的，比较完善的是epoll。<br />

### poll函数
```c
#include <poll.h>
int poll(struct pollfd* fds, nfds_t nfds, int timeout);
// fds: 传入的fd数组
// nfds: 监听数组的实际有效监听个数
// timeout: 超时

struct pollfd {
    int fd;         // 监听的fd
    short events;   // 监听的事件(读/写/异常)
    short revents;  // 返回的事件
}; // 事件: `POLLIN`(可读) / `POLLOUT`(可写) / `POLLERR`(异常)
```
timeout跟select不一样. (int)milliseconds毫秒<br />
- \>0: 等待指定时间<br />
- 0: 立即返回<br />
- -1: 阻塞等待 select是NULL<br />
返回值：满足对应监听事件的fd数量<br />
这改进也没啥卵用(笑)<br />

# epoll
只能用在linux上<br />

### epoll相关函数和结构 *重点
epoll : epoll file descriptor (句柄), 本质是一颗红黑树(平衡二叉树的一种)<br />
```c
int epoll_create(int size);
/*
    创建一颗监听红黑树(epoll树)
    size: 监听节点数量/传入的fd数量(只是个参考值)
    返回值: 成功, epoll句柄(指向epoll根节点的fd); 失败, -1, errno: EINVAL
*/

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
/*
    操作一颗epoll树
    epfd: 根节点fd
    op: 操作类型
        EPOLL_CTL_ADD: 添加一个fd(挂到树上)
        EPOLL_CTL_MOD: 修改一个fd
        EPOLL_CTL_DEL: 删除一个fd(从树上摘下, 即取消监听)
    fd: 要操作的fd
    event: 监听的事件 只是结构体ptr, 不是数组
    返回值: 成功, 0; 失败, -1, errno: EEXIST / ENOENT / EINVAL
*/

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);
/*
    等待epoll树上有事件发生
    epfd: 根节点fd
    events: 传出参数。监听的事件の【数组】(注意这有个s), 传出时, 里面是满足监听条件事件结构体の数组(没有废物)
        传入时, 里面是epoll_ctl传入的结构体, 里面是fd
        传出时, 里面是epoll_ctl传出的结构体, 里面是fd
        注意: 这个数组的大小必须大于等于maxevents
    maxevents: 数组大小
    timeout: 超时
        -1: 阻塞等待
        0: 立即返回
        >0: 等待指定时间(毫秒)
    返回值: 成功, 发生事件的数量(可以用作循环上限); 失败, -1, errno: EFAULT / EINTR / EINVAL / ENOMEM; 超时, 0
    发生事件的数量 <= maxevents
    发生事件的数量 <= events数组大小
    发生事件的数量 <= epoll树上发生的事件数量
    发生事件的数量 <= epoll树上所有fd的数量
*/

struct epoll_event {
    uint32_t events;      /* 监听的事件 EPOLLIN / EPOLLOUT / EPOLLERR / ... */
    epoll_data_t data;    /* 用户数据, 有fd */
};

typedef union epoll_data {
    void* ptr;
    int fd;               /* 传入的fd, 对应前面函数参数的fd */
    uint32_t u32; // 用不着
    uint64_t u64; // 用不着
} epoll_data_t;
```
<br />

### epoll使用
见epoll-server.c<br />
由于eopll监听的是fd，所以不一定只能在网络编程上使用，也可以用于进程间通信，比如监听pipe。<br />

### epoll事件模型
epoll有两种事件模型：<br />
- ET(边缘触发): 只在状态改变时通知一次, 之后不再通知, 需要自己循环读取<br />
- LT(水平触发, 默认): 只要状态改变, 就会通知, 直到状态改变为止<br />
想设置为ET模式怎么办？只需要：<br />
```c
event.events = EPOLLIN | EPOLLET;
```
或上即可。<br />
epoll的ET模式是非阻塞的, 也就是说, 你必须自己循环读取数据, 否则会一直阻塞在epoll_wait上<br />

### epoll优缺点
优点:<br />
- 高效。
- 支持大文件描述符数量(>1024)
- 支持边缘触发(ET)和水平触发(LT)两种模式
- ...<br />
缺点:<br />
- 不能跨平。只能在linux上使用，甚至Unix上都不行.<br />

# epoll反应堆模型
epoll ET模式 + 非阻塞 + void* ptr<br />
void* ptr被打造为与函数相关的指针(含函数指针的结构体指针, 要套娃), 也就是回调函数, 这样就可以实现反应堆模型了<br />
<br />
本来的流程类似于:<br />
```c
main() {
    // 1. 创建epoll树
    // 2. 创建监听fd
    // 3. 循环监听
        // 4. 等待事件发生
        // 5. 可读。读出数据
        // 6. 处理业务逻辑(处理数据)
        // 7. 把处理完的数据写回去
}
```
现在变成了:<br />
```c
main() {
    // 1. 创建epoll树
    // 2. 创建监听fd
    // 3. 循环监听
        // 4. 等待事件发生
        // 5. 可读。读出数据
        // 6. 处理业务逻辑(处理数据)
        // 7. 把结点从epoll树上摘下
        // 8. 回调函数放进结点, 事件改成EPOLLOUT
        // 9. 把结点放回epoll树上
        ...
        (下一次循环开始)
        // 10. 等待事件发生
        // 11. 可写。把处理完的数据写回去
        // 12. 把结点从epoll树上摘下
        // 13. 事件改成EPOLLIN
        // 14. 把结点放回epoll树上
        ...
        (下一次循环开始)
}
```
这样做是严谨的，而不是脱裤子放屁。<br />
为什么要把结点从epoll树上摘下再放回去？<br />
因为这是个红黑树，直接修改会破坏平衡性.<br />
见epoll-reactor-model.c<br />