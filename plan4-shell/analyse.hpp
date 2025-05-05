#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include "globalset.hpp"

class Entity;
class File;
class Command;
class Sign;
class CommandAnalyser;
class CommandExecutor;

void runCommand(std::string command_line) {
    CommandAnalyser cmd_ana(command_line);
    
}

class Entity {
public:
    int type;
    /*
        0: Sign
        1: File
        2: Command
    */ 
};

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
    // "|", "&", ">", "<", ">>"

    Sign(std::string sign) : sign(sign) {
        this->type = 0;
    }
};

class CommandAnalyser {
public:
    std::vector<std::string> command_line;
    std::vector<Entity*> command_entities;
    bool is_background = false;

    CommandAnalyser(std::string line) {
        std::string tmp;
        std::istringstream iss(line);
        while (iss >> tmp) {
            command_line.push_back(tmp);
        }
        if (command_line.back() == "&") {
            is_background = true;
            command_line.pop_back();
        }
        auto it = command_line.begin();
        auto it_last = it;
        for (; it != command_line.end(); ++it) {
            if (*it == "|") {
                // 管道两端都是命令
            }
            else if (*it == ">" || *it == ">>" || *it == "<") {
                // 重定向符号前面是命令，后面是文件
            }
        }
        if (command_entities.size() == 0) {
            command_entities.push_back(new Command(command_line));
        }
    }
};

class CommandExecutor {
public:
};