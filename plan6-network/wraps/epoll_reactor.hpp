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
#include "net.hpp"

namespace net {
    //const int MAX_EVENTS = 50;
    //const int MAX_CLNT = 128;
    const int max_open_files = 1024;
}

class epoll_reactor {
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
/* ------------------------------------------- */
    class event {
    private:
        epoll_reactor* reactor = nullptr;;
    public:
        int fd;
        int events;
        int status;
        void (*call_back)(int, void*);
        void* arg;
        char* buf;
        int buflen;
        time_t last_active;

        event() = delete;
        event(int fd, int events, void (*call_back)(int, void*), void* arg)
            : fd(fd), events(events), call_back(call_back), arg(arg) {
            this->reactor = nullptr;
            this->status = 0;
            this->buflen = 0;
            this->last_active = time(nullptr);
            this->buf = new char[reactor->buffer_size];
        }
        event(event&& other) = delete;
        event(const event&) = delete;
        event& operator=(const event&) = delete;
        event& operator=(event&& other) = delete;

        ~event() {
            delete[] buf;
            Close(fd);
        }

        void set(int fd, int events, void (*call_back)(int, void*), void* arg) {
            this->fd = fd;
            this->events = events;
            this->call_back = call_back;
            this->arg = arg;
            this->status = 0;
            this->buflen = 0;
            this->last_active = time(nullptr);
        }

        void set(int events) {
            this->events = events;
        }

        bool add_to(epoll_reactor* reactor) {
            if (this->reactor != nullptr) {
                return false;
            }
            this->reactor = reactor;
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
            if (epoll_ctl(reactor->epoll_fd, op, this->fd, &ev) < 0) {
                printf("Failed to add event: epoll_ctl error: %s\n", strerror(errno));
            } else {
                printf("Successfully added event: fd=%d, events=%d\n", this->fd, events);
            }
            return true;
        }

        void del_from_reactor() {
            if (this->reactor == nullptr) {
                return;
            }
            this->reactor = nullptr;
            this->status = 0;
            epoll_ctl(this->reactor->epoll_fd, EPOLL_CTL_DEL, this->fd, NULL);
            printf("Deleted event: fd=%d\n", this->fd);
        }
    };
/* ------------------------------------------- */

    event** events;
public:
    epoll_reactor() = delete;
    epoll_reactor(
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
        this->events = new event*[this->max_events + 1];
        for (int i = 0; i < this->max_events + 1; ++i) {
            this->events[i] = nullptr;
        }
    }
    epoll_reactor(const epoll_reactor&) = delete;
    epoll_reactor(epoll_reactor&&) = delete;
    epoll_reactor& operator=(const epoll_reactor&) = delete;
    epoll_reactor& operator=(epoll_reactor&&) = delete;

    ~epoll_reactor() {
        for (int i = 0; i < this->max_events + 1; ++i) {
            if (this->events[i] != nullptr) {
                delete this->events[i];
            }
        }
        delete[] this->events;
        Close(this->epoll_fd);
    }

    void listen_init() {
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
            if (this->listen_fd == -1) {
                printf("Failed to create socket: %s\n", strerror(errno));
                break;
            }
            int opt = 1;
            if (setsockopt(this->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
                printf("Failed to set socket options: %s\n", strerror(errno));
                break;
            }
            if (Bind(this->listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
                printf("Failed to bind socket: %s\n", strerror(errno));
                break;
            }
            if (Listen(this->listen_fd, this->max_clients) == -1) {
                printf("Failed to listen on socket: %s\n", strerror(errno));
                break;
            }
            event* pe = new event(this->listen_fd, EPOLLIN, accept_connection, this);
            if (pe->add_to(this) == false) {
                printf("Failed to add listen event: %s\n", strerror(errno));
                delete pe;
                break;
            }
            this->events[this->max_events] = pe;
            printf("Listening on %s:%d\n", this->ip.c_str(), this->port);
        } while (0);
    }

    // arg: ptr of epoll_reactor
    static void accept_connection(int lfd, void* arg) {
        epoll_reactor* reactor = static_cast<epoll_reactor*>(arg);
        if (reactor->listen_fd != lfd) {
            return;
        }
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_len = sizeof(clnt_addr);
        int i, cfd = Accept(lfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
        if (cfd == -1) {
            perr_exit("Accept");
        }
        event** ppe;
        event* pe;
        do {
            for (i = 0; i < reactor->max_events; ++i) {
                if (reactor->events[i] == nullptr) {
                    ppe = &reactor->events[i];
                    break;
                }
            }
            if (i == reactor->max_events) {
                printf("%s: too many clients\n", __func__);
                Close(cfd);
                break;
            }
            int flag = fcntl(cfd, F_GETFL, O_NONBLOCK);
            if (flag == -1) {
                printf("%s: failed to set non-blocking: %s\n", __func__, strerror(errno));
            }
            pe = new event(cfd, EPOLLIN | EPOLLET, recvdata, reactor);
            pe->add_to(reactor);
        } while (0);
    }

    static void recvdata(int fd, void* arg) {}

    static void senddata(int fd, void* arg) {}
};