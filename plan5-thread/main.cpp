#include <iostream>
#include "thread_pool.hpp"
#include <chrono>

void add(int a, int b) {
    std::cout << "a(" << a << ") + b(" << b << ") = " << a + b << std::endl;
}



int main() {
    thread_pool pool(4, 8);
    pool.init();

    for (int i = 0; i < 10; ++i) {
        pool.submit(add, i, i + 1);
    }
    pool.shutdown();
    return 0;
}