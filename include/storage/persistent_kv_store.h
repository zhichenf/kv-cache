#pragma once

#include "storage/kv_store.h"
#include "common/wal.h"
#include "common/snapshot.h"
#include <string>
#include <deque>
#include <chrono>
#include <mutex>
#include <atomic>

// ============================================================
// 滑动窗口：检测突发写入
// ============================================================
class SlidingWindow {
public:
    SlidingWindow(int max_ops, std::chrono::milliseconds window_size);
    
    // 记录一次操作
    void AddOp();
    
    // 检查是否超过阈值
    bool IsExceeded() const;
    
    // 获取当前窗口内的操作次数
    size_t Count() const;
    
    // 清空窗口
    void Clear();

private:
    int max_ops_;
    std::chrono::milliseconds window_size_;
    mutable std::mutex mutex_;
    std::deque<std::chrono::steady_clock::time_point> timestamps_;
};

// ============================================================
// 持久化 KV 存储
// ============================================================
class PersistentKvStore : public KvStore {
public:
    // 构造函数：指定数据目录
    explicit PersistentKvStore(const std::string& data_dir);
    
    // 析构函数：触发最终快照
    ~PersistentKvStore();
    
    // 禁止拷贝和移动
    PersistentKvStore(const PersistentKvStore&) = delete;
    PersistentKvStore& operator=(const PersistentKvStore&) = delete;
    
    // 重写 Set：写 WAL + 更新内存
    void Set(const std::string& key, const std::string& value);
    
    // 重写 Delete：写 WAL + 更新内存
    bool Delete(const std::string& key);
    
    // 手动触发快照
    void ForceSnapshot();
    
    // 获取操作计数
    uint64_t GetOpCount() const;
    
    // 获取快照间隔
    uint64_t GetSnapshotInterval() const;

private:
    // 从 WAL 和 Snapshot 恢复数据
    void Recover();
    
    // 检查是否需要触发快照
    void CheckSnapshotTrigger();
    
    // 创建快照（原子操作）
    void CreateSnapshot();
    
    std::string data_dir_;
    WAL& wal_;
    Snapshot& snapshot_;
    
    // 操作计数（原子，支持多线程并发）
    std::atomic<uint64_t> op_count_;
    
    // 快照间隔配置
    static constexpr uint64_t kSnapshotInterval = 500;      // 总操作阈值
    static constexpr int kBurstMaxOps = 100;                  // 突发操作阈值
    static constexpr int kBurstWindowMs = 1000;               // 滑动窗口大小（1秒）
    
    // 滑动窗口
    SlidingWindow burst_window_;
};
