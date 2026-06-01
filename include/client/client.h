#pragma once

#include <string>
#include <optional>
#include <cstdint>

// 封装 RESP 协议的网络客户端，提供 KV 操作的 C++ 接口
class KvClient {
public:
    KvClient() = default;
    ~KvClient();

    KvClient(const KvClient&) = delete;
    KvClient& operator=(const KvClient&) = delete;

    bool Connect(const std::string& host, uint16_t port);
    void Disconnect();

    bool Set(const std::string& key, const std::string& value);
    std::optional<std::string> Get(const std::string& key);
    bool Delete(const std::string& key);
    bool Exists(const std::string& key);
    size_t Keys();

private:
    std::string SendCommand(const std::string& cmd);
    int sock_fd_ = -1;
};
