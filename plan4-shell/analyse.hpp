#pragma once
#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "globalset.hpp"

class Entity;
class File;
class Command;
class Sign;
class CommandAnalyser;
class CommandExecutor;

void runCommand(std::string command_line);

class Entity {
public:
    int type;
    /*
        0: Sign
        1: File
        2: Command
    */ 
};

/*
    几类特殊的命令
    1. cd
    2. exit 不知道是不是特殊命令
    // 3. setenv   3-6没要求
    // 4. unsetenv
    // 5. export
    // 6. history
    7. jobs 7-9不会做
    8. fg
    9. bg
*/
class Command : public Entity {
public:
    std::string cmd_name;
    std::string cmd_path;
    std::vector<std::string> cmd_args;

    Command(std::vector<std::string> cmd_with_args) {
        this->type = 2;
        cmd_name = cmd_with_args[0];
        cmd_with_args.erase(cmd_with_args.begin());
        cmd_args = cmd_with_args;
    }
};

class File : public Entity {
public:
    std::string file_name;
    int file_fd;

    File(std::string name) : file_name(name) {
        this->type = 1;
        file_fd = open(file_name.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        // 处理错误暂且不考虑
        // if (file_fd < 0) {
        //     std::cerr << "Error opening file: " << file_name << std::endl;
        //     exit(1);
        // }
    }
};

class Sign : public Entity {
public:
    std::string sign;
    int stype;
    // "|", ">", "<", ">>"
    int pipe_fd[2]; // 管道fd
    // 剩下的跟着耗内存

    Sign(std::string sign) : sign(sign) {
        this->type = 0;
        if (sign == "|") {
            stype = 0;
        }
        else if (sign == ">") {
            stype = 1;
        }
        else if (sign == "<") {
            stype = 2;
        }
        else if (sign == ">>") {
            stype = 3;
        }
    }
};

class CommandAnalyser {
public:
    std::vector<std::string> command_line;
    std::deque<Entity*> command_entities;
    bool is_background = false;

    CommandAnalyser(std::string line) {
        std::string tmp;
        std::istringstream iss(line);
        // 后续优化，当用户不输入空格时，修改这里
        // 默认用户会自觉输入空格
        // 默认没有() && || &| &; &< &> &>> &<< 各种东西
        // cb1 | cb2 | cb3  cb内是命令、参数和重定向符号和后面文件
        while (iss >> tmp) {
            command_line.push_back(tmp);
        }
        if (command_line.back() == "&") {
            is_background = true;
            command_line.pop_back();
        }
        if (command_line.empty()) {
            return;
        }
        auto it = command_line.end() - 1;
        auto it_last = it;
        for (; it >= command_line.begin(); --it) { // 整体从后往前扫描
            if (*it == "|") {
                auto it2 = it_last;
                auto it_last2 = it2;
                for (; it2 != it; --it2) {
                    if (*it2 == ">" || *it2 == ">>" || *it2 == "<") {
                        command_entities.push_front(new File(*(it2 + 1)));
                        command_entities.push_front(new Sign(*it2));
                        it_last2 = it2 - 1;
                    }
                }
                command_entities.push_front(new Command(std::vector<std::string>(it + 1, it_last2 + 1)));
                command_entities.push_front(new Sign(*it));
                it_last = it - 1;
            }
        }
        // it 最后指向的位置是command_line第一个元素的前一个位置
        // it + 1 就是command_line.begin()
        command_entities.push_front(new Command(std::vector<std::string>(command_line.begin(), it_last + 1)));
    }

    ~CommandAnalyser() {
        for (auto entity : command_entities) {
            delete entity;
        }
    }
};

class CommandExecutor {
public:
};

void runCommand(std::string command_line) { // 这里会fork子进程
    pid_t newPGID;
    pid_t pid;
    CommandAnalyser cmd_ana(command_line);
    if (cmd_ana.command_entities.empty()) {
        return;
    }
    int n = cmd_ana.command_entities.size();
    int np_cnt = 0;

    auto& arr = cmd_ana.command_entities;
    for (int i = 0; i < n; ++i) { /* 第一遍，不创建子进程
                                否则子进程读不到下面部署的数据*/
        if (arr[i]->type == 1) { // file
            File* file = static_cast<File*>(arr[i]);
            // 处理文件重定向
            file->file_fd = open(file->file_name.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        }
        else if (arr[i]->type == 0) { // sign
            Sign* sign = static_cast<Sign*>(arr[i]);
            if (sign->stype == 0) { // pipe
                pipe(sign->pipe_fd); // 一般不会出错。先不考虑错误处理
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        if (arr[i]->type == 2) { // command
            Command* cmd = static_cast<Command*>(arr[i]);
            if (cmd->cmd_name == "cd") {
                // cd
                if (cmd->cmd_args.size() > 0) {
                    std::string new_dir = cmd->cmd_args[0];
                    cdset(new_dir);
                }
                return;
            }
            else if (cmd->cmd_name == "exit") {
                // exit
                std::cout << "Exiting shell." << std::endl;
                exit(0);
            }
            else {
                pid = fork();
                if (pid == 0) {
                    // 子进程
                    
                }
            }
        }
    }

    if (!cmd_ana.is_background && SHELL_PID != FG_PGID) {
        // 等待子进程结束
        pid_t wpid;
        while ((wpid = waitpid(-FG_PGID, nullptr, 0)) > 0);
    }

}