#include "storage/persistent_kv_store.h"
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================
// SlidingWindow 实现
// ============================================================

SlidingWindow::SlidingWindow(int max_ops, std::chrono::milliseconds window_size)
    : max_ops_(max_ops), window_size_(window_size) {
}

void SlidingWindow::AddOp() {
    std::lock_guard<std::mutex> lock(mutex_);
    timestamps_.push_back(std::chrono::steady_clock::now());
}

bool SlidingWindow::IsExceeded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - window_size_;
    
    // 从最新往旧的方向计数，遇到过期的就停
    size_t count = 0;
    for (auto it = timestamps_.rbegin(); it != timestamps_.rend(); ++it) {
        if (*it < cutoff) {
            break;
        }
        ++count;
    }
    
    return static_cast<int>(count) > max_ops_;
}

size_t SlidingWindow::Count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - window_size_;
    
    size_t count = 0;
    for (auto it = timestamps_.rbegin(); it != timestamps_.rend(); ++it) {
        if (*it < cutoff) {
            break;
        }
        ++count;
    }
    
    return count;
}

void SlidingWindow::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    timestamps_.clear();
}

// ============================================================
// PersistentKvStore 实现
// ============================================================

PersistentKvStore::PersistentKvStore(const std::string& data_dir)
    : data_dir_(data_dir),
      wal_(WAL::GetInstance()),
      snapshot_(Snapshot::GetInstance()),
      op_count_(0),
      burst_window_(kBurstMaxOps, std::chrono::milliseconds(kBurstWindowMs)) {
    
    // 创建数据目录
    fs::create_directories(data_dir_);
    
    // 初始化 WAL
    WALConfig config;
    config.policy = FsyncPolicy::NO;  // 教学阶段用 NO，后续可配置
    wal_.Init(data_dir_ + "/wal.log", config);
    
    // 恢复数据
    Recover();
}

PersistentKvStore::~PersistentKvStore() {
    // 析构时不再触发快照，简化退出逻辑
}

void PersistentKvStore::Recover() {
    // 1. 尝试加载快照
    if (snapshot_.IsValid(data_dir_)) {
        std::vector<std::pair<std::string, std::string>> entries;
        if (snapshot_.Load(data_dir_, entries)) {
            std::lock_guard<std::shared_mutex> lock(mutex_);
            for (const auto& [key, value] : entries) {
                data_[key] = value;
            }
        }
    }
    
    // 2. 回放 WAL（WAL 内部有锁）
    auto records = wal_.ReadAll();
    std::lock_guard<std::shared_mutex> lock(mutex_);
    for (const auto& record : records) {
        switch (record.op) {
            case OpType::SET:
                data_[record.key] = record.value;
                break;
            case OpType::DEL:
                data_.erase(record.key);
                break;
        }
    }
}

void PersistentKvStore::Set(const std::string& key, const std::string& value) {
    // 1. 先写 WAL
    wal_.AppendSet(key, value);
    
    // 2. 更新内存
    {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        data_[key] = value;
    }
    
    // 3. 更新计数并检查快照触发
    ++op_count_;
    burst_window_.AddOp();
    CheckSnapshotTrigger();
}

bool PersistentKvStore::Delete(const std::string& key) {
    // 1. 先写 WAL
    wal_.AppendDel(key);
    
    // 2. 更新内存
    bool result = false;
    {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        result = data_.erase(key) > 0;
    }
    
    // 3. 更新计数并检查快照触发
    ++op_count_;
    burst_window_.AddOp();
    CheckSnapshotTrigger();
    
    return result;
}

void PersistentKvStore::CheckSnapshotTrigger() {
    bool should_snapshot = false;
    
    // 条件1：总操作次数超过阈值
    if (op_count_ >= kSnapshotInterval) {
        should_snapshot = true;
    }
    
    // 条件2：突发写入检测
    if (burst_window_.IsExceeded()) {
        should_snapshot = true;
    }
    
    if (should_snapshot) {
        CreateSnapshot();
    }
}

void PersistentKvStore::CreateSnapshot() {
    // 原子操作顺序：
    // 1. 锁 WAL（阻止后台线程和新写入）
    // 2. 锁 KvStore 写锁（阻止内存修改）
    // 3. 获取数据
    // 4. 创建快照
    // 5. 清空 WAL（不加锁版本）
    // 6. 重置计数器
    // 7. 释放锁
    
    std::unique_lock<std::mutex> wal_lock(wal_.GetMutex());
    std::unique_lock<std::shared_mutex> kv_lock(mutex_);
    
    // 构建快照数据
    std::vector<std::pair<std::string, std::string>> snapshot_data;
    snapshot_data.reserve(data_.size());
    for (const auto& [key, value] : data_) {
        snapshot_data.emplace_back(key, value);
    }
    
    // 创建快照
    if (!snapshot_data.empty()) {
        snapshot_.Create(data_dir_, snapshot_data);
    }
    
    // 清空 WAL（不加锁版本，因为我们已经持有 wal_lock）
    wal_.ClearWithoutLock();
    
    // 重置计数器
    op_count_ = 0;
    burst_window_.Clear();
}

void PersistentKvStore::ForceSnapshot() {
    CreateSnapshot();
}

uint64_t PersistentKvStore::GetOpCount() const {
    return op_count_;
}

uint64_t PersistentKvStore::GetSnapshotInterval() const {
    return kSnapshotInterval;
}
