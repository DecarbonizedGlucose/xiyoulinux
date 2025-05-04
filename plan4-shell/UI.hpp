#pragma once
#include "globalset.hpp"
#include <stdio.h>
#include <iostream>

#define CM_END "\033[0m"
#define B_BLUE "\033[34;1m"
#define B_RED "\033[31;1m"
#define B_DFT "\033[1m"
#define F_GREEN "\033[36m"

void printShellHeader(){
    getDisplayDir();
    std::cout << F_GREEN << "┌──(" << CM_END;
    std::cout << B_BLUE << USR_NAME << '@' << HOST_NAME << CM_END;
    std::cout << F_GREEN << ")-[" << CM_END;
    std::cout << B_DFT << CUR_PATH_DISPLAY << CM_END;
    std::cout << F_GREEN << ']' << CM_END << std::endl;
    std::cout << F_GREEN << "└─" << CM_END;
    std::cout << B_BLUE << '$' << CM_END << ' ';
}