#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

// 线程安全的 KV 存储引擎，基于 shared_mutex 实现读写分离
class KvStore {
public:
    KvStore() = default;
    ~KvStore() = default;

    KvStore(const KvStore&) = delete;
    KvStore& operator=(const KvStore&) = delete;

    void Set(const std::string& key, const std::string& value);
    std::optional<std::string> Get(const std::string& key) const;
    bool Delete(const std::string& key);
    bool Exists(const std::string& key) const;
    std::vector<std::string> AllKeys() const;
    size_t Size() const;

protected:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};
