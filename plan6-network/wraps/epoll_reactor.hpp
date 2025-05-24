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
#include "net.hpp"

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
private:
    reactor* parent_reactor = nullptr;
public:
    int fd = -1;
    int events;
    int status = 0; // 0: not in reactor, 1: in reactor
    void* buf = nullptr;
    int buflen = 0;
    time_t last_active;
    // 守旧派
    void (*call_back_func_c)(int, void*) = nullptr;
    void* arg = this;
    // 维新派
    std::function<void()> call_back_func_cpp = nullptr;

    event() = delete;
    event(int fd, int events, void (*call_back)(int, void*), void* arg);
    event(int fd, int events, void (*call_back)(int, void*));
    event(event&& other) = delete;
    event(const event&) = delete;
    event& operator=(const event&) = delete;
    event& operator=(event&& other) = delete;
    ~event();
    void set(int fd, int events, void (*call_back)(int, void*), void* arg);
    void set(int fd, int events, void (*call_back)(int, void*));
    void set(int fd, int events, std::function<void()> call_back_func);
    void set_events(int events);
    bool add_to(reactor* parent_reactor);
    void del_from_reactor();
    static void recvdata(int fd, void* arg);
    static void senddata(int fd, void* arg);
    static void acceptconn(int lfd, void* arg);
    void call_back();
};

reactor::event::event(int fd, int events, void (*call_back)(int, void*), void* arg)
    : fd(fd), events(events), call_back_func_c(call_back), arg(arg), parent_reactor(nullptr),
    status(0), buflen(0), last_active(time(nullptr)) {
    this->buf = new char[parent_reactor->buffer_size];
}

reactor::event::event(int fd, int events, void (*call_back)(int, void*))
    : fd(fd), events(events), call_back_func_c(call_back), arg(arg), parent_reactor(nullptr),
    status(0), buflen(0), last_active(time(nullptr)) {
    this->buf = new char[parent_reactor->buffer_size];
}

reactor::event::~event() {
    delete[] buf;
    Close(fd);
}

void reactor::event::set(int fd, int events, void (*call_back)(int, void*), void* arg) {
    this->fd = fd;
    this->events = events;
    this->call_back_func_c = call_back;
    this->arg = arg;
    this->status = 0;
    this->buflen = 0;
    this->last_active = time(nullptr);
}

void reactor::event::set(int fd, int events, void (*call_back)(int, void*)) {
    this->fd = fd;
    this->events = events;
    this->call_back_func_c = call_back;
    this->status = 0;
    this->buflen = 0;
    this->last_active = time(nullptr);
}

void reactor::event::set(int fd, int events, std::function<void()> call_back_func) {
    this->fd = fd;
    this->events = events;
    this->call_back_func_cpp = call_back_func;
    this->status = 0;
    this->buflen = 0;
    this->last_active = time(nullptr);
    this->call_back_func_c = nullptr; // 清除旧的回调函数
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

void reactor::event::recvdata(int fd, void* arg) {
    reactor::event* ev = static_cast<reactor::event*>(arg);
    ssize_t nread = Read(ev->fd, ev->buf, ev->parent_reactor->buffer_size);
    do {
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "非阻塞读取数据时没有数据可读." << std::endl;
                return; // 没有数据可读
            }
            std::cerr << "读取数据失败: " << strerror(errno) << std::endl;
            ev->del_from_reactor();
            break;
        }
        else if (nread == 0) {
            std::cout << "客户端关闭连接: fd=" << ev->fd << std::endl;
            ev->del_from_reactor();
            break; // 客户端关闭连接
        }
        ev->del_from_reactor();
        ev->buflen = nread;
        static_cast<char*>(ev->buf)[nread] = '\0'; // 确保字符串结束
        /* ----- 处理接收的数据的业务逻辑部分 -----*/

        /* ----------------------------------- */
        //ev->set_events(EPOLLOUT | EPOLLET); // 设置为可写事件?
        ev->add_to(ev->parent_reactor); // 重新添加到反应堆
        return;
    } while (0);
    Close(ev->fd);
}

void reactor::event::senddata(int fd, void* arg) {}

void reactor::event::call_back() {
        if (call_back_func_cpp) {
            call_back_func_cpp();
        } else if (call_back_func_c) {
            call_back_func_c(fd, arg);
        } else {
            std::cerr << fd << "的事件没有回调函数" << std::endl;
        }
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
        event* pe = new event(this->listen_fd, EPOLLIN, event::acceptconn);
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