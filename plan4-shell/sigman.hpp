#pragma once
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include "globalset.hpp"

void globalSignalSet();
void handler_Ignored(int sig);
void handler_SigInt(int sig);

void globalSignalSet() {
    struct sigaction sig_to_shell;
    sig_to_shell.sa_flags = 0;
    sigemptyset(&sig_to_shell.sa_mask);
    sig_to_shell.sa_handler = handler_SigInt;

    sigaction(SIGINT, &sig_to_shell, NULL); // 处理Ctrl-C
    sigaction(SIGTSTP, &sig_to_shell, NULL); // 处理Ctrl-Z
    sigaction(SIGCONT, &sig_to_shell, NULL); // 处理Ctrl-Z后再Ctrl-C

    struct sigaction sig_to_ignore;
    sig_to_ignore.sa_flags = 0;
    sigemptyset(&sig_to_ignore.sa_mask);
    sig_to_ignore.sa_handler = handler_Ignored;

    struct sigaction sig_force_quit;
    sig_force_quit.sa_flags = 0;
    sigemptyset(&sig_force_quit.sa_mask);
    sig_force_quit.sa_handler = SIG_DFL;
    sigaction(SIGQUIT, &sig_force_quit, NULL);  /* 处理Ctrl-\ */
}

void handler_Ignored(int sig) {
    // 忽略信号
}

void handler_ForceQuit(int sig) {
    /* 处理Ctrl-\
       忽略 */
    eof_without_ctrl_D = true;
}

void handler_SigInt(int sig) {
    pid_t pid = getpid();
    if (pid == SHELL_PID) {
        // shell等待输入时Ctrl-C
        eof_without_ctrl_D = true;
    }
    else {
        // 子进程运行时Ctrl-C
        kill(pid, SIGINT);
    }
}