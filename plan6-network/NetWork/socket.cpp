#include "socket.h"

TcpSocket::TcpSocket(int sockfd) : sockfd_(sockfd) {}

TcpSocket::~TcpSocket() {
    close(sockfd_);
}

int TcpSocket::sendMsg(std::string msg) {
    if (msg.empty()) {
        std::cerr << "Error: Cannot send an empty message." << std::endl;
        return -1;
    }
    uint32_t len = htonl(msg.size());
    int n = write(sockfd_, &len, sizeof(len));
    if (n != sizeof(len)) {
        std::cerr << "Error: Failed to send message length." << std::endl;
        return -1;
    }
    int ret = write(sockfd_, msg.c_str(), msg.size());
    return ret < 0 ? -1 : 0;
}

int TcpSocket::recvMsg(std::string& msg) {
    uint32_t len = 0;
    int n = read(sockfd_, &len, sizeof(len));
    if (n != sizeof(len)) {
        return -1;
    }
    len = ntohl(len);
    msg.resize(len);
    size_t recved = 0;
    while (recved < len) {
        n = read(sockfd_, &msg[recved], len - recved);
        if (n <= 0) {
            return -1; // 读取失败或连接关闭
        }
        recved += n;
    }
    return 0;
}
