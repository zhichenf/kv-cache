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
#include <errno.h>
#endif

#include <cstring>

// 构造，记录端口号
TcpServer::TcpServer(uint16_t port, size_t thread_count)
    : port_(port), thread_count_(thread_count) {
}

// 析构，关闭服务
TcpServer::~TcpServer() {
    Stop();
}

// 注册消息回调，收到客户端数据时调用
void TcpServer::SetOnMessage(OnMessageCallback cb) {
    on_message_ = std::move(cb);
}

// 设置 socket 为非阻塞模式
void TcpServer::SetNonBlocking(int fd) {
#ifdef __linux__
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#elif defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
#endif
}

// 创建监听 socket → bind → listen → 启动 Reactor
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

    // 创建线程池
    thread_pool_ = std::make_unique<ThreadPool>(thread_count_);
    LOG_INFO("Thread pool started with " + std::to_string(thread_pool_->Size()) + " threads");

#ifdef __linux__
    SetNonBlocking(server_fd_);

    // 创建 epoll 实例
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        return false;
    }

    // 注册 server_fd 到 epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

    // 启动事件循环线程
    std::thread(&TcpServer::EventLoop, this).detach();
    
    LOG_INFO("Reactor (epoll) started on port " + std::to_string(port_));
#elif defined(_WIN32)
    // 创建 IOCP
    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (iocp_ == NULL) {
        return false;
    }

    // 启动 Worker 线程池（IOCP 完成端口线程）
    DWORD iocp_threads = std::thread::hardware_concurrency();
    if (iocp_threads == 0) iocp_threads = 4;
    
    for (DWORD i = 0; i < iocp_threads; i++) {
        worker_threads_.emplace_back(&TcpServer::WorkerThread, this);
    }

    // 启动 accept 线程（阻塞 accept，每个连接一个）
    accept_thread_ = std::make_unique<std::thread>(&TcpServer::AcceptLoop, this);
    
    LOG_INFO("Reactor (IOCP) started on port " + std::to_string(port_));
#endif

    return true;
}

#ifdef __linux__
// Linux: epoll 事件循环
void TcpServer::EventLoop() {
    while (running_) {
        int n = epoll_wait(epoll_fd_, events_, 1024, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events_[i].data.fd == server_fd_) {
                HandleAccept();
            } else if (events_[i].events & EPOLLIN) {
                HandleRead(events_[i].data.fd);
            } else if (events_[i].events & EPOLLOUT) {
                HandleWrite(events_[i].data.fd);
            }
        }
    }
}
#endif

#ifdef _WIN32
// Windows: 阻塞 accept 循环（在独立线程中运行）
void TcpServer::AcceptLoop() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = static_cast<int>(
            ::accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len));
        
        if (client_fd < 0) {
            if (!running_) break;
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        int port = ntohs(client_addr.sin_port);
        LOG_INFO("New connection from " + std::string(ip) + ":" + std::to_string(port) +
                 " (fd=" + std::to_string(client_fd) + ")");

        // 创建连接上下文
        auto ctx = std::make_unique<ConnectionContext>();
        ctx->fd = client_fd;
        ctx->is_read = true;
        ctx->wsa_buf.buf = ctx->buffer;
        ctx->wsa_buf.len = sizeof(ctx->buffer);
        std::memset(&ctx->overlapped, 0, sizeof(OVERLAPPED));
        
        // 保存原始指针用于 WSARecv
        ConnectionContext* raw_ctx = ctx.get();
        
        {
            std::lock_guard lock(contexts_mutex_);
            contexts_[client_fd] = std::move(ctx);
        }
        
        // 关联到 IOCP
        CreateIoCompletionPort((HANDLE)(UINT_PTR)client_fd, iocp_, client_fd, 0);
        
        // 发起异步读（使用原始指针）
        DWORD flags = 0;
        WSARecv(client_fd, &raw_ctx->wsa_buf, 1, NULL, &flags,
                &raw_ctx->overlapped, NULL);
    }
}

// Windows: IOCP Worker 线程
void TcpServer::WorkerThread() {
    while (running_) {
        DWORD bytes;
        ULONG_PTR key;
        OVERLAPPED* overlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            iocp_, &bytes, &key, &overlapped, INFINITE);

        if (!ok) {
            int err = GetLastError();
            if (key != server_fd_) {
                CloseConnection(static_cast<int>(key));
            }
            continue;
        }

        if (bytes == 0) {
            // 对端关闭
            if (key != server_fd_) {
                CloseConnection(static_cast<int>(key));
            }
            continue;
        }

        int fd = static_cast<int>(key);

        std::lock_guard lock(contexts_mutex_);
        auto it = contexts_.find(fd);
        if (it == contexts_.end()) continue;
        
        auto& ctx = it->second;
        if (!ctx) continue;

        if (ctx->is_read) {
            // 读完成，获取数据
            std::string data(ctx->buffer, bytes);
            
            // 重新发起异步读, 这个读是异步读取下一个命令
            ctx->wsa_buf.buf = ctx->buffer;
            ctx->wsa_buf.len = sizeof(ctx->buffer);
            DWORD flags = 0;
            WSARecv(fd, &ctx->wsa_buf, 1, NULL, &flags,
                    &ctx->overlapped, NULL);
            
            // 提交到线程池处理
            thread_pool_->Submit([this, fd, data]() {
                if (on_message_) {
                    std::string resp = on_message_(fd, data);
                    if (!resp.empty()) {
                        ::send(fd, resp.data(), static_cast<int>(resp.size()), 0);
                    }
                }
            });
        }
    }
}
#endif

// 处理新连接（仅 Linux）
void TcpServer::HandleAccept() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (true) {
        int client_fd = static_cast<int>(
            ::accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len));
        
        if (client_fd < 0) {
            break;
        }

        SetNonBlocking(client_fd);

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        int port = ntohs(client_addr.sin_port);
        LOG_INFO("New connection from " + std::string(ip) + ":" + std::to_string(port) +
                 " (fd=" + std::to_string(client_fd) + ")");

#ifdef __linux__
        // 注册到 epoll
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
#endif
    }
}

// 处理读事件（仅 Linux，Windows 用 IOCP）
void TcpServer::HandleRead(int client_fd) {
    char chunk[4096];
    
    while (true) {
        int n = ::recv(client_fd, chunk, sizeof(chunk), 0);
        
        if (n < 0) {
#ifdef __linux__
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#elif defined(_WIN32)
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) break;
#endif
            CloseConnection(client_fd);
            return;
        }
        
        if (n == 0) {
            CloseConnection(client_fd);
            return;
        }

        if (!on_message_) {
            continue;
        }

        // 复制数据，提交到线程池处理
        std::string data(chunk, static_cast<size_t>(n));
        
        thread_pool_->Submit([this, client_fd, data]() {
            std::string resp = on_message_(client_fd, data);
            if (!resp.empty()) {
                ::send(client_fd, resp.data(), static_cast<int>(resp.size()), 0);
            }
        });
    }
}

// 处理写事件
void TcpServer::HandleWrite(int client_fd) {
    std::lock_guard lock(write_mutex_);
    auto it = write_buffers_.find(client_fd);
    if (it == write_buffers_.end()) return;

    while (!it->second.empty()) {
        int n = ::send(client_fd, it->second.data(),
                       static_cast<int>(it->second.size()), 0);
        if (n < 0) {
#ifdef __linux__
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#elif defined(_WIN32)
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) break;
#endif
            CloseConnection(client_fd);
            return;
        }
        it->second.erase(0, n);
    }

    if (it->second.empty()) {
        write_buffers_.erase(it);
#ifdef __linux__
        // 取消关注写事件
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
#endif
    }
}

// 关闭连接
void TcpServer::CloseConnection(int client_fd) {
#ifdef __linux__
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
#elif defined(_WIN32)
    {
        std::lock_guard lock(contexts_mutex_);
        contexts_.erase(client_fd);
    }
#endif
    
    ::close(client_fd);
    
    {
        std::lock_guard lock(write_mutex_);
        write_buffers_.erase(client_fd);
    }
    
    LOG_DEBUG("Client disconnected (fd=" + std::to_string(client_fd) + ")");
}

// 停止服务
void TcpServer::Stop() {
    running_ = false;

    // 停止线程池
    if (thread_pool_) {
        thread_pool_.reset();
    }

#ifdef __linux__
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
#elif defined(_WIN32)
    if (iocp_ != INVALID_HANDLE_VALUE) {
        // 向每个 Worker 投递完成包以唤醒它们
        for (auto& t : worker_threads_) {
            PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);
        }
        CloseHandle(iocp_);
        iocp_ = INVALID_HANDLE_VALUE;
    }
    
    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_.clear();
    
    if (accept_thread_ && accept_thread_->joinable()) {
        accept_thread_->join();
    }
    accept_thread_.reset();
#endif

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
}
