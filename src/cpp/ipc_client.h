#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>

class IPCClient {
public:
    using MessageCallback = std::function<void(const std::string& msg)>;

    IPCClient(const std::string& socketPath);
    ~IPCClient();

    bool connect();
    void disconnect();
    void send(const std::string& msg);
    void setMessageCallback(MessageCallback cb);

    bool isConnected() const { return connected_; }

private:
    void runLoop();
    bool readLine(std::string& line);

    std::string socketPath_;
    int sockfd_ = -1;
    std::atomic<bool> connected_{false};
    std::atomic<bool> stop_{false};
    std::thread thread_;
    MessageCallback callback_;
};
