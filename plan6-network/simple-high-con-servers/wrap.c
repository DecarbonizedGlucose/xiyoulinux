#include "wrap.h"

void perr_exit(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE); // 1
}

int Accept(int fd, struct sockaddr* sa, socklen_t* salenptr) {
    int n;
again:
    if ((n = accept(fd, sa, salenptr)) < 0) {
        if ((errno == ECONNABORTED) || (errno == EINTR)) {
            goto again;
        }
        else {
            perr_exit("Accept error");
        }
    }
    return n;
}

int Bind(int fd, const struct sockaddr* sa, socklen_t salen) {
    int n;
    if ((n = bind(fd, sa, salen)) < 0) {
        perr_exit("Bind error");
    }
    return n;
}

int Connect(int fd, const struct sockaddr* sa, socklen_t salen) {
    int n;
    if ((n = connect(fd, sa, salen)) < 0) {
        perr_exit("Connect error");
    }
    return n;
}

int Listen(int fd, int backlog) {
    int n;
    if ((n = listen(fd, backlog)) < 0) {
        perr_exit("Listen error");
    }
    return n;
}

int Socket(int family, int type, int protocol) {
    int n;
    if ((n = socket(family, type, protocol)) < 0) {
        perr_exit("Socket error");
    }
    return n;
}

ssize_t Read(int fd, void* ptr, size_t nbytes) {
    ssize_t n;
again:
    if ((n = read(fd, ptr, nbytes)) == -1) {
        if (errno == EINTR) {
            goto again;
        }
        else {
            return -1;
        }
    }
    return n;
}

ssize_t Write(int fd, const void* ptr, size_t nbytes) {
    ssize_t n;
again:
    if ((n = write(fd, ptr, nbytes)) == -1) {
        if (errno == EINTR) {
            goto again;
        }
        else {
            return -1;
        }
    }
    return n;
}

int Close(int fd) {
    int n;
    if ((n = close(fd)) < 0) {
        perr_exit("Close error");
    }
    return n;
}

ssize_t Readn(int fd, void* vptr, size_t nbytes) {
    size_t nleft;
    ssize_t nread;
    char* ptr;

    ptr = (char*)vptr;
    nleft = nbytes;
    while (nleft > 0) {
        if ((nread = Read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR) {
                nread = 0; // call read again
            }
            else {
                return -1;
            }
        }
        else if (nread == 0) {
            break; // EOF
        }
        nleft -= nread;
        ptr += nread;
    }
    return (nbytes - nleft);
}

ssize_t Writen(int fd, const void* vptr, size_t nbytes) {
    size_t nleft;
    ssize_t nwritten;
    const char* ptr;

    ptr = (const char*)vptr;
    nleft = nbytes;
    while (nleft > 0) {
        if ((nwritten = Write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR) {
                nwritten = 0; // call write again
            }
            else {
                return -1;
            }
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return nbytes;
}

static ssize_t my_read(int fd, char* ptr) {
    static int read_cnt;
    static char* read_ptr;
    static char read_buf[100];

    if (read_cnt <= 0) {
again:
        if ((read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
            if (errno == EINTR) {
                goto again;
            }
            return -1;
        }
        else if (read_cnt == 0) {
            return 0; // EOF
        }
        read_ptr = read_buf;
    }
    read_cnt--;
    *ptr = *read_ptr++;
    return 1;
}

ssize_t Readline(int fd, void* vptr, size_t maxlen) {
    ssize_t n, rc;
    char c, *ptr;
    ptr = (char*)vptr;

    for (n = 1; n < maxlen; n++) {
        if ((rc = my_read(fd, &c)) == 1) {
            *ptr++ = c;
            if (c == '\n') {
                break;
            }
        }
        else if (rc == 0) {
            *ptr = 0;
            return n - 1;
        }
        else {
            return -1; // error
        }
    }
    *ptr = 0;
    return n;
}