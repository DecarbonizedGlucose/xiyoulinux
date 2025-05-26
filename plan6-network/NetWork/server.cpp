#include "server.h"

TcpServer::TcpServer()
    : listenfd_(socket(AF_INET, SOCK_STREAM, 0)), socket_(nullptr) {}

TcpServer::~TcpServer() {
    if (listenfd_ != -1) {
        close(listenfd_);
    }
}

bool TcpServer::setListen(unsigned short port) {
    if (listenfd_ < 0) {
        perror("Socket creation failed");
        return false;
    }
    socket_.reset(new TcpSocket(listenfd_));
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    int opt = 1;
    printf("ip: %s, port: %d\n", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
    setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(listenfd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Bind failed in setListen");
        return false;
    }
    listen(listenfd_, 5);
    return true;
}

bool TcpServer::acceptConn() {
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    int cfd = accept(listenfd_, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
    if (cfd == -1) {
        perror("Accept failed");
        return false;
    }
    return true;
}
