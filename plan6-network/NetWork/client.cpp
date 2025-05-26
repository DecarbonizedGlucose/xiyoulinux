#include "client.h"

TcpClient::TcpClient()
    : fd_(socket(AF_INET, SOCK_STREAM, 0)),
        socket_(std::make_unique<TcpSocket>(fd_)) {}

TcpClient::~TcpClient() {
    close(fd_);
}

bool TcpClient::connectToHost(const char* ip, unsigned short port) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        return false;
    }
    if (connect(fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return false;
    }
    return true;
}
