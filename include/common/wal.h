#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

// 操作类型枚举
enum class OpType : uint8_t {
    SET = 1,    // 设置键值对
    DEL = 2,    // 删除键
};

// 刷盘策略枚举
enum class FsyncPolicy : uint8_t {
    ALWAYS = 0,     // 每条命令都 fsync（最安全，最慢）
    EVERYSEC = 1,   // 每秒 fsync 一次（均衡，默认推荐）
    NO = 2,         // 不主动 fsync（最快，可能丢数据）
};

// WAL 配置结构
struct WALConfig {
    FsyncPolicy policy = FsyncPolicy::EVERYSEC;  // 刷盘策略
    int sync_interval_ms = 1000;                  // 刷盘间隔（毫秒），仅 EVERYSEC 有效
};

// WAL 记录结构
struct WALRecord {
    OpType op;              // 操作类型
    std::string key;        // 键
    std::string value;      // 值（DEL 操作时为空）
    
    // 序列化为二进制数据（用于写入文件）
    std::string Serialize() const;
    
    // 从二进制数据反序列化（用于读取文件）
    static WALRecord Deserialize(const char* data, size_t len, size_t& consumed);
};

// WAL 类（单例模式）
class WAL {
public:
    // 获取单例实例
    static WAL& GetInstance();
    
    // 禁止拷贝和移动
    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;
    WAL(WAL&&) = delete;
    WAL& operator=(WAL&&) = delete;
    
    // 初始化 WAL（启动时调用）
    void Init(const std::string& filepath, const WALConfig& config = {});
    
    // 关闭 WAL（停止后台线程，关闭文件）
    void Shutdown();
    
    // 追加一条日志记录
    void Append(const WALRecord& record);
    
    // 追加 SET 操作
    void AppendSet(const std::string& key, const std::string& value);
    
    // 追加 DEL 操作
    void AppendDel(const std::string& key);
    
    // 读取所有日志记录（用于崩溃恢复）
    std::vector<WALRecord> ReadAll();
    
    // 清空日志文件（快照后调用）
    void Clear();
    
    // 手动刷盘（强制写入磁盘）
    void Sync();
    
    // 获取日志文件路径
    std::string GetFilepath() const;
    
    // 获取日志文件大小
    size_t GetFileSize() const;
    
    // 检查 WAL 是否启用
    bool IsEnabled() const;
    
    // 获取当前刷盘策略
    FsyncPolicy GetPolicy() const;

private:
    // 私有构造函数（单例模式）
    WAL();
    ~WAL();
    
    // 尝试打开文件
    bool TryOpen();
    
    // 关闭文件
    void CloseFD();
    
    // 后台刷盘线程
    void SyncLoop();
    
    // 写入一条完整的记录（包含 CRC32）
    void WriteRecord(const WALRecord& record);
    
    // 互斥锁（保护所有操作）
    mutable std::mutex mutex_;
    
    // 配置
    WALConfig config_;
    
    // 文件相关（全部使用原生系统调用）
    std::string filepath_;
    int fd_;                    // 原生文件描述符
    off_t write_pos_;           // 当前写入位置
    bool is_open_;
    bool enabled_;
    int max_retries_;
    
    // 后台线程
    std::thread sync_thread_;
    std::atomic<bool> running_;
};
