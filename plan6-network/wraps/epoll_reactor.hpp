#pragma once
#include <ctime>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cctype>
#include <string>
#include <functional>
#include <memory>
#include <iostream>
#include "func_wrapper.hpp"
#include "net.hpp"

template<typename F, typename... Args>
auto func_wrapper(F&& f, Args&&... args)
-> std::function<decltype(std::forward<F>(f)(std::forward<Args>(args)...))()> {
    using return_type = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
    std::function<return_type()> binded_func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    return binded_func();
}

namespace net {
    //const int MAX_EVENTS = 50;
    //const int MAX_CLNT = 128;
    const int max_open_files = 1024;
}

class reactor {
private:
    short port;
    sa_family_t family;
    std::string ip;
    int buffer_size;
    int epoll_fd;
    int listen_fd;
    int max_events;
    int max_clients;
    int epoll_timeout;
    class event;
    event** events;
    struct epoll_event* epoll_events;
    bool running = false;
public:
    reactor() = delete;
    reactor(std::string ip, short port, sa_family_t family = AF_INET,
                  int buffer_size = BUFSIZ, int max_events = 50,
                  int max_clients = 128, int epoll_timeout = 1000);
    reactor(const reactor&) = delete;
    reactor(reactor&&) = delete;
    reactor& operator=(const reactor&) = delete;
    reactor& operator=(reactor&&) = delete;
    ~reactor();
    void listen_init();
    void start();
    void stop();
    bool is_running() const { return running; }
    int get_epoll_fd() const { return epoll_fd; }
    int get_listen_fd() const { return listen_fd; }
};

class reactor::event {
public:
    reactor* parent_reactor = nullptr;
    int fd = -1;
    int events;
    int status = 0; // 0: not in reactor, 1: in reactor
    char* buf = nullptr;
    int buflen = 0;
    int buffer_size = 0; // 缓冲区大小
    time_t last_active;
    // 维新派
    std::function<void()> call_back_func_cpp = nullptr;

    event() = delete;
    event(int fd, int events, int buffer_size, std::function<void()> call_back_func);
    event(int fd, int events, int buffer_size);
    event(event&& other) = delete;
    event(const event&) = delete;
    event& operator=(const event&) = delete;
    event& operator=(event&& other) = delete;
    ~event();
    void set(int fd, int events, std::function<void()> call_back_func);
    void set_events(int events);
    bool add_to(reactor* parent_reactor);
    void del_from_reactor();
    static void recvdata(int fd, event* pev, std::function<int(char*)> rtask, std::function<int(char*)> wtask);
    static void senddata(int fd, event* pev, std::function<int(char*)> rtask, std::function<int(char*)> wtask);
    static void accept_connection(int lfd, event* pev);
    void call_back();
    bool is_buf_full() const;
};

reactor::event::event(int fd, int events, int buffer_size, std::function<void()> call_back_func)
    : fd(fd), events(events), call_back_func_cpp(call_back_func), buffer_size(buffer_size),
    status(0), buflen(0), last_active(time(nullptr)) {
    this->buf = new char[buffer_size = 1];
    this->buf[buffer_size] = '\0';
}

reactor::event::event(int fd, int events, int buffer_size)
    : fd(fd), events(events), buffer_size(buffer_size),
    status(0), buflen(0), last_active(time(nullptr)) {
    this->buf = new char[buffer_size + 1];
    this->buf[buffer_size] = '\0';
}

reactor::event::~event() {
    delete[] buf;
    Close(fd);
}

void reactor::event::set(int fd, int events, std::function<void()> call_back_func) {
    this->fd = fd;
    this->events = events;
    this->call_back_func_cpp = call_back_func;
    this->status = 0;
    this->buflen = 0;
    this->last_active = time(nullptr);
}

void reactor::event::set_events(int events) {
    this->events = events;
}

bool reactor::event::add_to(reactor* parent_reactor) {
    if (this->parent_reactor != nullptr) {
        return false;
    }
    this->parent_reactor = parent_reactor;
    struct epoll_event ev = {0, {0}};
    int op = 0;
    ev.data.ptr = this;
    ev.events = this->events;
    if (this->status == 0) { // 说明是新增的事件
        op = EPOLL_CTL_ADD;
        this->status = 1;
    } else {
        op = EPOLL_CTL_MOD;
    }
    if (epoll_ctl(parent_reactor->epoll_fd, op, this->fd, &ev) < 0) {
        std::cerr << "添加事件失败: epoll_ctl error:" << strerror(errno) << std::endl;
    } else {
        std::cout << "添加事件: fd=" << this->fd << std::endl;
    }
    return true;
}

void reactor::event::del_from_reactor() {
    if (this->parent_reactor == nullptr) {
        return;
    }
    this->parent_reactor = nullptr;
    this->status = 0;
    if (epoll_ctl(this->parent_reactor->epoll_fd, EPOLL_CTL_DEL, this->fd, NULL) < 0) {
        std::cerr << "移除事件失败: epoll_ctl error:" << strerror(errno) << std::endl;
    }
    else {
        std::cout << "移除事件: fd=" << this->fd << std::endl;
    }
}

void reactor::event::recvdata(int fd, event* pev, std::function<int(char*)> rtask, std::function<int(char*)> wtask) {
    ssize_t nread = Read(pev->fd, pev->buf, pev->buffer_size);
    do {
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "非阻塞读取数据时没有数据可读." << std::endl;
                return; // 没有数据可读
            }
            std::cerr << "读取数据失败: " << strerror(errno) << std::endl;
            pev->del_from_reactor();
            break;
        }
        else if (nread == 0) {
            std::cout << "客户端关闭连接: fd=" << pev->fd << std::endl;
            pev->del_from_reactor();
            break; // 客户端关闭连接
        }
        pev->del_from_reactor();
        pev->buflen = nread;
        int next_event;
        /* ----- 处理接收的数据的业务逻辑部分 -----*/
        if (pev->is_buf_full()) {
            std::cerr << "缓冲区已满，无法继续接收数据." << std::endl;
            next_event = EPOLLIN | EPOLLET; // 继续监听可读事件
            rtask(pev->buf); // 调用任务处理函数
        }
        else {
            next_event = rtask(pev->buf); // 调用任务处理函数
        }
        if (next_event & EPOLLIN) {
            pev->call_back_func_cpp = func_wrapper(event::recvdata, pev->fd, pev, rtask, wtask);
        }
        else {
            pev->call_back_func_cpp = func_wrapper(event::senddata, pev->fd, pev, rtask, wtask);
        }
        /* ----------------------------------- */
        pev->set_events(next_event);
        pev->add_to(pev->parent_reactor); // 重新添加到反应堆
        return;
    } while (0);
    Close(pev->fd);
}

void reactor::event::senddata(int fd, event* pev, std::function<int(char*)> rtask, std::function<int(char*)> wtask) {
    ssize_t nread = Write(pev->fd, pev->buf, pev->buflen);
    do {
        if (nread == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "非阻塞写入数据时没有数据可写." << std::endl;
                return; // 没有数据可写
            }
            std::cerr << "写入数据失败: " << strerror(errno) << std::endl;
            pev->del_from_reactor();
            break;
        }
        pev->del_from_reactor();
        int next_event = wtask(pev->buf); // 调用任务处理函数
        if (next_event & EPOLLIN) {
            pev->call_back_func_cpp = func_wrapper(event::recvdata, pev->fd, pev, rtask, wtask);
        }
        else {
            pev->call_back_func_cpp = func_wrapper(event::senddata, pev->fd, pev, rtask, wtask);
        }
        pev->set_events(next_event);
        pev->add_to(pev->parent_reactor); // 重新添加到反应堆
        return;
    } while (0);
}

void reactor::event::accept_connection(int lfd, event* pev) {
    int i;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    int cfd = Accept(lfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
    do {
        if (cfd < 0) {
            std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
            break;
        }
        std::cout << "新连接: fd=" << cfd << ", 客户端地址=" << inet_ntoa(clnt_addr.sin_addr) << std::endl;
        for (i = 0; i < pev->parent_reactor->max_events; ++i) {
            if (pev->parent_reactor->events[i] == nullptr) {
                break; // 找到一个空闲的事件槽
            }
        }
        if (i == pev->parent_reactor->max_events) {
            std::cerr << "没有足够的事件槽来处理新连接." << std::endl;
            break;
        }
        int flag = fcntl(cfd, F_GETFL, O_NONBLOCK);
        if (flag == -1) {
            std::cerr << "设置非阻塞失败: " << strerror(errno) << std::endl;
            break;
        }
        pev->parent_reactor->events[i] = new event(cfd, EPOLLIN | EPOLLET, pev->buffer_size);
        /*
            设置回调函数
            funcs还没写完
        */
        return;
    } while (0);
    Close(cfd);
}

void reactor::event::call_back() {
    if (this->call_back_func_cpp) {
        this->call_back_func_cpp(); // 调用C++风格的回调函数
    } else {
        std::cerr << "没有设置回调函数." << std::endl;
    }
}

bool reactor::event::is_buf_full() const {
    return (buflen > 0 && buf[buflen - 1] == '\0');
}

reactor::reactor(
    std::string ip = "", short port = 8080, sa_family_t family = AF_INET,
    int buffer_size = BUFSIZ, int max_events = 50,
    int max_clients = 128, int epoll_timeout = 1000)
    : family(family), ip(ip), port(port),
    buffer_size(buffer_size), max_events(max_events),
    max_clients(max_clients), epoll_timeout(epoll_timeout) {
    this->epoll_fd = epoll_create(net::max_open_files);
    if (this->epoll_fd == -1) {
        perr_exit(__func__);
    }
    this->listen_fd = -1;
    this->epoll_events = new struct epoll_event[this->max_events];
    this->events = new event*[this->max_events + 1];
    for (int i = 0; i < this->max_events + 1; ++i) {
        this->events[i] = nullptr;
    }
}

reactor::~reactor() {
    for (int i = 0; i < this->max_events + 1; ++i) {
        delete this->events[i];
    }
    delete[] this->events;
    Close(this->listen_fd);
}

void reactor::listen_init() {
    struct sockaddr_in serv_addr;
    if (this->ip.empty()) {
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    }
    else {
        serv_addr.sin_addr.s_addr = inet_addr(this->ip.c_str());
    }
    serv_addr.sin_family = this->family;
    serv_addr.sin_port = htons(this->port);
    do {
        this->listen_fd = Socket(this->family, SOCK_STREAM, 0);
        if (this->listen_fd < 0) {
            std::cerr << "Failed to create socket:" << strerror(errno) << std::endl;
            break;
        }
        int opt = 1;
        if (setsockopt(this->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
            break;
        }
        if (Bind(this->listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Bind error: " << strerror(errno) << std::endl;
            break;
        }
        if (Listen(this->listen_fd, this->max_clients) < 0) {
            std::cerr << "Listen error: " << strerror(errno) << std::endl;
            break;
        }
        event* pe = new event(this->listen_fd, (int)EPOLLIN, this->buffer_size);
        pe->call_back_func_cpp = func_wrapper(event::accept_connection, this->listen_fd, pe);
        if (pe->add_to(this) == false) {
            std::cerr << "Failed to add listen event to reactor: " << strerror(errno) << std::endl;
            delete pe;
            break;
        }
        return;
    } while (0);
    Close(this->listen_fd);
}

void reactor::start() {
    if (this->listen_fd < 0) {
        std::cout << "监听socket未初始化." << std::endl;
        return;
    }
    if (this->running) {
        std::cout << "反应堆已经启动." << std::endl;
        return;
    }
    this->running = true;
    std::cout << "反应堆已启动, 监听 " << this->ip << ":" << this->port << std::endl;

    while (this->running) {
        int nready = epoll_wait(this->epoll_fd, this->epoll_events, this->max_events, this->epoll_timeout);
        if (nready < 0) {
            if (errno == EINTR) {
                continue; // 被信号中断，继续等待
            }
            std::cerr << "Epoll等待时发生异常." << std::endl;
            break;
        } else if (nready == 0) {
            continue; // 超时，继续等待
        }
        for (int i = 0; i < nready; ++i) {
            event* ev = static_cast<event*>(this->epoll_events[i].data.ptr);
            if (ev) {
                ev->call_back();
            }
        }
    }
}

void reactor::stop() {
    if (!this->running) {
        std::cerr << "反应堆未启动." << std::endl;
        return;
    }
    this->running = false;
    // 关闭监听socket和epoll
    Close(this->listen_fd);
    Close(this->epoll_fd);
    std::cout << "反应堆已停止." << std::endl;
}