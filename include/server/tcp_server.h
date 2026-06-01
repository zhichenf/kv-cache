#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include <thread>
#include <memory>
#include <atomic>

// 只负责建立 TCP 连接和收发原始字节流，不涉及任何协议解析
class TcpServer {
public:
    // 接收原始字节流，返回响应。client_fd 用于上层区分不同连接
    using OnMessageCallback = std::function<std::string(
        int client_fd, const std::string& data)>;

    explicit TcpServer(uint16_t port);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void SetOnMessage(OnMessageCallback cb);
    bool Start();
    void Stop();

private:
    void AcceptLoop();
    void HandleClient(int client_fd);

    uint16_t port_;                        // 监听端口
    int server_fd_ = -1;                   // 监听 socket fd
    std::atomic<bool> running_{false};     // 运行标志
    std::unique_ptr<std::thread> accept_thread_;        // accept 循环线程
    OnMessageCallback on_message_;         // 数据回调
#ifdef _WIN32
    bool wsa_initialized_ = false;         // WinSock 初始化标志
#endif
};
