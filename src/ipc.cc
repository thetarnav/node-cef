#include "ipc.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

IPCClient::IPCClient(const std::string& socketPath) : socketPath_(socketPath) {}

IPCClient::~IPCClient() {
    disconnect();
}

bool IPCClient::connect() {
    sockfd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        perror("socket");
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    if (::connect(sockfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    connected_ = true;
    stop_ = false;
    thread_ = std::thread(&IPCClient::runLoop, this);
    return true;
}

void IPCClient::disconnect() {
    stop_ = true;
    connected_ = false;
    if (sockfd_ >= 0) {
        shutdown(sockfd_, SHUT_RDWR);
        close(sockfd_);
        sockfd_ = -1;
    }
    if (thread_.joinable()) {
        thread_.detach();
    }
}

void IPCClient::send(const std::string& msg) {
    if (!connected_) return;
    std::string line = msg + "\n";
    ssize_t written = ::write(sockfd_, line.c_str(), line.size());
    if (written < 0) {
        perror("write");
        disconnect();
    }
}

void IPCClient::setMessageCallback(MessageCallback cb) {
    callback_ = std::move(cb);
}

void IPCClient::runLoop() {
    while (!stop_ && connected_) {
        std::string line;
        if (readLine(line)) {
            if (callback_) {
                callback_(line);
            }
        } else {
            break;
        }
    }
    disconnect();
}

bool IPCClient::readLine(std::string& line) {
    line.clear();
    char ch;
    while (!stop_) {
        ssize_t n = recv(sockfd_, &ch, 1, 0);
        if (n <= 0) {
            if (n == 0) {
                std::cerr << "IPC server closed connection" << std::endl;
            } else {
                perror("recv");
            }
            return false;
        }
        if (ch == '\n') {
            break;
        }
        line.push_back(ch);
    }
    return true;
}