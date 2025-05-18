#define _POSIX_C_SOURCE 200112L // sigaction
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include "wrap.h"

#define MAXLINE 80
#define SERV_PORT 9000

void func_sigcld(int sig) {
    while (waitpid(0, NULL, WNOHANG) > 0);
}

char buf[BUFSIZ];

int main() {
    int lfd = 0, cfd = 0, ret;
    struct sockaddr_in serv_addr, clit_addr;
    pid_t pid;

    struct sigaction act;
    act.sa_handler = func_sigcld;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);
    
    socklen_t caddrlen = sizeof(clit_addr);
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = SERV_PORT;
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    lfd = Socket(AF_INET, SOCK_STREAM, 0);

    Bind(lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    Listen(lfd, 20);
    printf("Accepting connentions...\n");

    while (1) {
        cfd = Accept(lfd, (struct sockaddr*)&clit_addr, &caddrlen);

        pid = fork();
        if (pid == 0) {
            // 子进程
            Close(lfd);
            while (1) {
                int n = Read(cfd, buf, MAXLINE);
                if (n == 0) {
                    printf("Client closed.\n");
                    break;
                }
                for (int i=0; i<n; ++i) {
                    buf[i] = toupper(buf[i]);
                }
                Write(cfd, buf, MAXLINE);
            }
            Close(cfd);
        }
        else if (pid > 0) {
            // 父进程
            Close(cfd);
        }
        else {
            perr_exit("fork error");
        }
    }

    Close(lfd);
    return 0;
}