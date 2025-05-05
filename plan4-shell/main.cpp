#include "analyse.hpp"
#include "sigman.hpp"
#include "globalset.hpp"
#include "UI.hpp"
#include <iostream>

int main() {

    info_preset();
    globalSignalSet();
    
    // shell 主循环
    while (1) {
        printShellHeader();
        readLineCommand();
        // 分析和输出
        // 包括创建子进程、分组、执行命令等
        // 父进程等待子进程结束
        std::cout << std::endl;
    }
    return 0;
}