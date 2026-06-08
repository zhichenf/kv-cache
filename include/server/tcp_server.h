#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "common/thread_pool.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// 只负责建立 TCP 连接和收发原始字节流，不涉及任何协议解析
class TcpServer {
public:
    // 接收原始字节流，返回响应。client_fd 用于上层区分不同连接
    using OnMessageCallback = std::function<std::string(
        int client_fd, const std::string& data)>;

    explicit TcpServer(uint16_t port, size_t thread_count = 0);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void SetOnMessage(OnMessageCallback cb);
    bool Start();
    void Stop();

private:
    // 设置 socket 为非阻塞模式
    void SetNonBlocking(int fd);
    
    // 处理新连接
    void HandleAccept();
    
    // 处理读事件（在线程池中执行）
    void HandleRead(int client_fd);
    
    // 处理写事件
    void HandleWrite(int client_fd);
    
    // 关闭连接
    void CloseConnection(int client_fd);

#ifdef __linux__
    // Linux: epoll 事件循环
    void EventLoop();
    int epoll_fd_ = -1;
    struct epoll_event events_[1024];
#elif defined(_WIN32)
    // Windows: 阻塞 accept 线程 + IOCP Worker 线程
    void AcceptLoop();
    void WorkerThread();
    HANDLE iocp_ = INVALID_HANDLE_VALUE;
    std::vector<std::thread> worker_threads_;
    std::unique_ptr<std::thread> accept_thread_;
    
    // 连接上下文
    struct ConnectionContext {
        OVERLAPPED overlapped;
        WSABUF wsa_buf;
        char buffer[4096];
        int fd;
        bool is_read;
    };
    
    std::unordered_map<int, std::unique_ptr<ConnectionContext>> contexts_;
    std::mutex contexts_mutex_;
#endif

    uint16_t port_;                        // 监听端口
    int server_fd_ = -1;                   // 监听 socket fd
    std::atomic<bool> running_{false};     // 运行标志
    OnMessageCallback on_message_;         // 数据回调
    
    // 线程池
    std::unique_ptr<ThreadPool> thread_pool_;
    size_t thread_count_;
    
    // 写缓冲区
    std::unordered_map<int, std::string> write_buffers_;
    std::mutex write_mutex_;
    
#ifdef _WIN32
    bool wsa_initialized_ = false;         // WinSock 初始化标志
#endif
};
