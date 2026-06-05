#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// 快照头信息
struct SnapshotHeader {
    static constexpr char MAGIC[5] = {'K', 'V', 'S', 'N', 'P'};
    static constexpr uint32_t VERSION = 1;
    
    uint32_t entry_count;  // 条目数量
};

// 快照条目
struct SnapshotEntry {
    std::string key;
    std::string value;
    
    // 序列化为二进制数据
    std::string Serialize() const;
    
    // 从二进制数据反序列化
    static SnapshotEntry Deserialize(const char* data, size_t len, size_t& consumed);
};

// 快照管理器（单例模式）
class Snapshot {
public:
    // 获取单例实例
    static Snapshot& GetInstance();
    
    // 禁止拷贝和移动
    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    Snapshot(Snapshot&&) = delete;
    Snapshot& operator=(Snapshot&&) = delete;
    
    // 创建快照
    // data_dir: 数据目录
    // entries: 要保存的键值对
    // 返回: 是否成功
    bool Create(const std::string& data_dir, 
                const std::vector<std::pair<std::string, std::string>>& entries);
    
    // 加载快照
    // data_dir: 数据目录
    // entries: 输出参数，加载的键值对
    // 返回: 是否成功加载
    bool Load(const std::string& data_dir,
              std::vector<std::pair<std::string, std::string>>& entries);
    
    // 检查快照文件是否存在且完整
    bool IsValid(const std::string& data_dir) const;
    
    // 获取快照文件路径
    std::string GetSnapshotPath(const std::string& data_dir) const;
    
    // 清除快照文件
    bool Remove(const std::string& data_dir);

private:
    // 私有构造函数（单例模式）
    Snapshot() = default;
    
    // 写入快照文件
    bool WriteSnapshot(const std::string& filepath,
                       const std::vector<std::pair<std::string, std::string>>& entries);
    
    // 读取快照文件
    bool ReadSnapshot(const std::string& filepath,
                      std::vector<std::pair<std::string, std::string>>& entries);
    
    // 计算 CRC32
    uint32_t CalculateCRC32(const char* data, size_t len) const;
};
