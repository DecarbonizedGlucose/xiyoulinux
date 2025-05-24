#include <iostream>
#include "epoll_reactor.hpp"

int main() {
    std::unique_ptr<reactor> rea = std::make_unique<reactor>();
    rea->listen_init();
    rea->start();
    while (rea->is_running()) {
    }
    rea->stop();
    return 0;
}