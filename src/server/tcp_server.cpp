#include "server/tcp_server.h"
#include "common/logger.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#endif

#include <cstring>

// 构造，记录端口号
TcpServer::TcpServer(uint16_t port)
    : port_(port) {
}

// 析构，关闭服务
TcpServer::~TcpServer() {
    Stop();
}

// 注册消息回调，收到客户端数据时调用
void TcpServer::SetOnMessage(OnMessageCallback cb) {
    on_message_ = std::move(cb);
}

// 创建监听 socket → bind → listen → 启动 AcceptLoop 线程
bool TcpServer::Start() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
    wsa_initialized_ = true;
#endif

    server_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (server_fd_ < 0) {
        return false;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (::bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return false;
    }

    if (::listen(server_fd_, SOMAXCONN) < 0) {
        return false;
    }

    running_ = true;
    accept_thread_ = std::make_unique<std::thread>(&TcpServer::AcceptLoop, this);
    return true;
}

void TcpServer::Stop() {
    running_ = false;
    if (server_fd_ >= 0) {
#ifdef _WIN32
        closesocket(server_fd_);
#else
        ::close(server_fd_);
#endif
        server_fd_ = -1;
    }
#ifdef _WIN32
    if (wsa_initialized_) {
        WSACleanup();
        wsa_initialized_ = false;
    }
#endif
    if (accept_thread_ && accept_thread_->joinable()) {
        accept_thread_->join();
    }
}

// 循环 accept 等待客户端连接，每接受一个连接就开新线程处理
void TcpServer::AcceptLoop() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = static_cast<int>(
            ::accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len));
        if (client_fd < 0) {
            if (!running_) {
                break;
            }
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        int port = ntohs(client_addr.sin_port);
        LOG_INFO("New connection from " + std::string(ip) + ":" + std::to_string(port) +
                 " (fd=" + std::to_string(client_fd) + ")");

        std::thread(&TcpServer::HandleClient, this, client_fd).detach();
    }
}

// 循环 recv 客户端原始字节流 → 传到 on_message_ 回调 → send 回调返回的响应
void TcpServer::HandleClient(int client_fd) {
    char chunk[4096];
    while (running_) {
        int n = ::recv(client_fd, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            break;
        }

        if (!on_message_) {
            continue;
        }

        std::string resp = on_message_(client_fd,
                                       std::string(chunk, static_cast<size_t>(n)));
        if (!resp.empty()) {
            ::send(client_fd, resp.data(), static_cast<int>(resp.size()), 0);
        }
    }

    LOG_DEBUG("Client disconnected (fd=" + std::to_string(client_fd) + ")");

#ifdef _WIN32
    closesocket(client_fd);
#else
    ::close(client_fd);
#endif
}
