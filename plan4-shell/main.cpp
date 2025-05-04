#include "analyse.hpp"
#include "sigman.hpp"
#include "globalset.hpp"
#include "UI.hpp"
#include <stdio.h>

static char maninput[BUFFER_SIZE];

void readStdin() {
    fflush(stdin);
    fgets(maninput, BUFFER_SIZE, stdin);
    char* p = strchr(maninput, '\n');
    if (p != nullptr) {
        *p = '\0';
    }
    fflush(stdin);
}

int main() {

    preset();
    
    // shell 主循环
    while (1) {
        printShellHeader();
        readStdin();
        // 分析和输出
        putchar('\n');
    }
    return 0;
}