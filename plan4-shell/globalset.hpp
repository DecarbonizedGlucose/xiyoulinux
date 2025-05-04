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

pid_t SHELL_PID;
pid_t FG_PGID;
std::vector<pid_t> BG_PGID_ARR;

void sys_err(const char* str) {
    perror(str);
    exit(1);
}

void setDisplayDir() {
    if (CUR_PATH.find(USR_HOME_PATH) == std::string::npos) {
        CUR_PATH_DISPLAY = CUR_PATH;
    }
    else {
        CUR_PATH_DISPLAY = "~" + CUR_PATH.substr(USR_HOME_PATH.length());
    }
}

void preset() {
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
}

void cdset(std::string new_dir) {
    if (chdir(new_dir.c_str()) != 0) {
        std::cout << "cd : 没有这个目录" << std::endl;
        return;
    }
    LAST_PATH = CUR_PATH;
    CUR_PATH = new_dir;
    setDisplayDir();
}