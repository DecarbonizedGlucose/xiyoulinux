# 响应式-I/O多路复用
**select** / **poll** / **epoll** 多路io转接

# select

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
nfd代表所有被监听的fd最大值+1,是循环上限: for `[1, nfd)`，所以是循环次数+1.
三个fd_set*都是传入传出参数
传入多个，返回可能没那么多，因为只返回检测到有反应的
readfds只检测其读事件，其余同理
timeval{秒,毫秒}，参照manpage即可
timeout:
- NULL  表示阻塞
- \> 0   监听超时时长
- 0     立即返回
返回值：
- 成功：返回检测到的fd数量
- 失败：返回-1，errno设置为EINTR或EBADF
- 超时：返回0

fd_set本质是位图(Bitmap)，所以提供了操作函数：
```c
void FD_CLR(int fd, fd_set* set); // 移除一个fd
int FD_ISSET(int fd, fd_set* set); // 判断
void FD_SET(int fd, fd_set* set); // 添加一个fd
void FD_ZERO(fd_set* set); // 清空
```

### select思路分析
```c
int main() {
    lfd = socket();
    bind();
    listen();
    fs_set rset, allset;
    FD_ZERO(&allset);
    FD_ZERO(&rset);
    FD_SET(lfd, &rset);
    ret = select(lfd + 1, &rset, NULL, NULL, NULL);
    if (ret > 0) {
        if (FD_ISSET(lfd, &rset)) {
            cfd = accept();
            FD_SET(cfd, &allset);
        }
    }
}
```