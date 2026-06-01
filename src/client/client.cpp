#include "client/client.h"
#include "common/protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <cstring>

// 析构，断开连接
KvClient::~KvClient() {
    Disconnect();
}

// 连接服务端，host 支持 IP 地址，port 为服务端端口
bool KvClient::Connect(const std::string& host, uint16_t port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
#endif

    sock_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (sock_fd_ < 0) {
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());

    if (::connect(sock_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        closesocket(sock_fd_);
#else
        ::close(sock_fd_);
#endif
        sock_fd_ = -1;
        return false;
    }

    return true;
}

// 断开与服务端的连接
void KvClient::Disconnect() {
    if (sock_fd_ >= 0) {
#ifdef _WIN32
        closesocket(sock_fd_);
#else
        ::close(sock_fd_);
#endif
        sock_fd_ = -1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

// 发送 RESP 命令并接收完整响应，返回原始 RESP 响应字符串
std::string KvClient::SendCommand(const std::string& cmd) {
    if (sock_fd_ < 0) {
        return "";
    }

    ::send(sock_fd_, cmd.data(), cmd.size(), 0);

    std::string buf;
    char chunk[4096];

    while (true) {
        if (!buf.empty()) {
            char type = buf[0];
            size_t crlf = buf.find("\r\n");

            if ((type == '+' || type == '-' || type == ':') && crlf != std::string::npos) {
                return buf;
            }

            if (type == '$' && crlf != std::string::npos) {
                int len = std::stoi(buf.substr(1, crlf - 1));
                if (len == -1) {
                    return buf;
                }
                size_t total = crlf + 2 + static_cast<size_t>(len) + 2;
                if (buf.size() >= total) {
                    return buf;
                }
            }
        }

        int n = ::recv(sock_fd_, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            return "";
        }
        buf.append(chunk, static_cast<size_t>(n));
    }
}

// 发送 SET 命令，true 表示成功
bool KvClient::Set(const std::string& key, const std::string& value) {
    Command cmd{CommandType::SET, {key, value}};
    std::string resp = SendCommand(SerializeCommand(cmd));
    return resp.size() >= 4 && resp[0] == '+';
}

// 发送 GET 命令，返回 optional，nullopt 表示 key 不存在
std::optional<std::string> KvClient::Get(const std::string& key) {
    Command cmd{CommandType::GET, {key}};
    std::string resp = SendCommand(SerializeCommand(cmd));
    if (resp.empty() || resp[0] != '$') {
        return std::nullopt;
    }

    size_t crlf = resp.find("\r\n");
    if (crlf == std::string::npos) {
        return std::nullopt;
    }

    int len = std::stoi(resp.substr(1, crlf - 1));
    if (len == -1) {
        return std::nullopt;
    }

    return resp.substr(crlf + 2, static_cast<size_t>(len));
}

// 发送 DEL 命令，true 表示 key 存在且已删除
bool KvClient::Delete(const std::string& key) {
    Command cmd{CommandType::DEL, {key}};
    std::string resp = SendCommand(SerializeCommand(cmd));
    if (resp.empty() || resp[0] != ':') {
        return false;
    }
    return resp[1] != '0';
}

// 发送 EXISTS 命令，true 表示 key 存在
bool KvClient::Exists(const std::string& key) {
    Command cmd{CommandType::EXISTS, {key}};
    std::string resp = SendCommand(SerializeCommand(cmd));
    return resp.size() >= 4 && resp[0] == '+';
}

// 发送 KEYS 命令，返回键总数
size_t KvClient::Keys() {
    Command cmd{CommandType::KEYS, {}};
    std::string resp = SendCommand(SerializeCommand(cmd));
    if (resp.empty() || resp[0] != ':') {
        return 0;
    }
    return static_cast<size_t>(std::stoi(resp.substr(1)));
}
