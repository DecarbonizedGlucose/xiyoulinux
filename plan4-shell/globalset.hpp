#pragma once
#define _BSD_SOURCE 1
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string>
#include <iostream>

#define BUFFER_SIZE 8192
#define ARR_SIZE 1024

std::string USR_NAME;
std::string HOST_NAME;
std::string USR_HOME_PATH;
std::string CUR_PATH;
std::string CUR_PATH_DISPLAY;
std::string LAST_PATH;

// 好像用不到？
//struct stat *DIR_STAT_ARR = new struct stat[ARR_SIZE];

static std::string lineinput;
bool eof_without_ctrl_D = false;

pid_t SHELL_PID;
pid_t FG_PGID;
std::vector<pid_t> BG_PGID_ARR;

void sys_err(const char* str) {
    perror(str);
    exit(1);
}

void readLineCommand() {
    std::cin.sync(); // 相当于 fflush(stdin)
    std::cin.clear(); // 清除输入流的错误标志
    std::getline(std::cin, lineinput);
    if (std::cin.eof()) {
        if (eof_without_ctrl_D) {
            /* Ctrl-C and Ctrl-\ */
            eof_without_ctrl_D = false;
            std::cout << std::endl;
        }
        else {
            // Ctrl+D
            std::cout << std::endl;
            std::cout << "Exit caused by Ctrl-D." << std::endl;
            std::cout << "Thanks for using." << std::endl;
            exit(0);
        }
    }
    if (lineinput.back() == '\n') {
        lineinput.pop_back();
    }
    std::cin.sync();
}

void setDisplayDir() {
    if (CUR_PATH.find(USR_HOME_PATH) == std::string::npos) {
        CUR_PATH_DISPLAY = CUR_PATH;
    }
    else {
        CUR_PATH_DISPLAY = "~" + CUR_PATH.substr(USR_HOME_PATH.length());
    }
}

void info_preset() {
    char _cur_path[BUFFER_SIZE];
    char _host_name[BUFFER_SIZE];
    getcwd(_cur_path, BUFFER_SIZE - 1);
    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    USR_NAME = pw->pw_name;
    gethostname(_host_name, BUFFER_SIZE - 1);
    CUR_PATH = _cur_path;
    USR_HOME_PATH = "/home/" + USR_NAME;
    setDisplayDir();
    HOST_NAME = _host_name;
    SHELL_PID = getpid(); // 这时候shell_pid就是fg_pid
    FG_PGID = getpgrp(); // 因为shell是前台进程组长
}

int cdset(std::string new_dir) {
    if (chdir(new_dir.c_str()) != 0) {
        std::cout << "cd : 没有这个目录" << std::endl;
        return -1;
    }
    LAST_PATH = CUR_PATH;
    CUR_PATH = new_dir;
    setDisplayDir();
    return 0;
}