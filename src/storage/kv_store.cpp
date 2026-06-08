#include "storage/kv_store.h"

// 写入键值对，已存在的 key 会被覆盖
void KvStore::Set(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    data_.insert_or_assign(key, value);
}

// 读取 key 对应的值，key 不存在返回 nullopt
std::optional<std::string> KvStore::Get(const std::string& key) const {
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    return it->second;
}

// 删除 key，返回 true 表示 key 存在且已删除
bool KvStore::Delete(const std::string& key) {
    std::unique_lock lock(mutex_);
    return data_.erase(key) > 0;
}

// 检查 key 是否存在
bool KvStore::Exists(const std::string& key) const {
    std::shared_lock lock(mutex_);
    return data_.count(key) > 0;
}

// 返回所有 key 的列表
std::vector<std::string> KvStore::AllKeys() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(data_.size());
    for (const auto& [key, _] : data_) {
        keys.push_back(key);
    }
    return keys;
}

// 返回当前存储的键值对数量
size_t KvStore::Size() const {
    std::shared_lock lock(mutex_);
    return data_.size();
}
