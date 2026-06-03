#include <gtest/gtest.h>
#include "common/wal.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// 测试用的临时文件路径
class WALTest : public ::testing::Test {
protected:
    std::string test_file = "test_wal.log";
    
    void SetUp() override {
        // 确保 WAL 已关闭
        WAL::GetInstance().Shutdown();
        // 删除旧的测试文件
        if (fs::exists(test_file)) {
            fs::remove(test_file);
        }
    }
    
    void TearDown() override {
        // 关闭 WAL
        WAL::GetInstance().Shutdown();
        // 清理测试文件
        if (fs::exists(test_file)) {
            fs::remove(test_file);
        }
    }
};

// 测试 WALRecord 序列化和反序列化
TEST_F(WALTest, RecordSerializeDeserialize) {
    // 测试 SET 操作
    WALRecord record1{OpType::SET, "name", "Alice"};
    std::string data1 = record1.Serialize();
    
    size_t consumed = 0;
    WALRecord restored1 = WALRecord::Deserialize(data1.data(), data1.size(), consumed);
    
    EXPECT_EQ(consumed, data1.size());
    EXPECT_EQ(restored1.op, OpType::SET);
    EXPECT_EQ(restored1.key, "name");
    EXPECT_EQ(restored1.value, "Alice");
    
    // 测试 DEL 操作
    WALRecord record2{OpType::DEL, "age", ""};
    std::string data2 = record2.Serialize();
    
    consumed = 0;
    WALRecord restored2 = WALRecord::Deserialize(data2.data(), data2.size(), consumed);
    
    EXPECT_EQ(consumed, data2.size());
    EXPECT_EQ(restored2.op, OpType::DEL);
    EXPECT_EQ(restored2.key, "age");
    EXPECT_EQ(restored2.value, "");
}

// 测试 WAL 追加和读取
TEST_F(WALTest, AppendAndReadAll) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    WAL::GetInstance().Init(test_file, config);
    
    // 追加几条记录
    WAL::GetInstance().AppendSet("name", "Alice");
    WAL::GetInstance().AppendSet("age", "25");
    WAL::GetInstance().AppendDel("name");
    
    // 读取所有记录
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    
    ASSERT_EQ(records.size(), 3);
    EXPECT_EQ(records[0].op, OpType::SET);
    EXPECT_EQ(records[0].key, "name");
    EXPECT_EQ(records[0].value, "Alice");
    
    EXPECT_EQ(records[1].op, OpType::SET);
    EXPECT_EQ(records[1].key, "age");
    EXPECT_EQ(records[1].value, "25");
    
    EXPECT_EQ(records[2].op, OpType::DEL);
    EXPECT_EQ(records[2].key, "name");
    EXPECT_EQ(records[2].value, "");
}

// 测试 WAL 清空
TEST_F(WALTest, Clear) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    WAL::GetInstance().Init(test_file, config);
    
    // 追加一些记录
    WAL::GetInstance().AppendSet("key1", "value1");
    WAL::GetInstance().AppendSet("key2", "value2");
    
    // 验证有记录
    EXPECT_EQ(WAL::GetInstance().ReadAll().size(), 2);
    
    // 清空
    WAL::GetInstance().Clear();
    
    // 验证为空
    EXPECT_EQ(WAL::GetInstance().ReadAll().size(), 0);
}

// 测试 WAL 文件大小
TEST_F(WALTest, FileSize) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    WAL::GetInstance().Init(test_file, config);
    
    // 初始大小为 0
    EXPECT_EQ(WAL::GetInstance().GetFileSize(), 0);
    
    // 追加记录后大小增加
    WAL::GetInstance().AppendSet("name", "Alice");
    EXPECT_GT(WAL::GetInstance().GetFileSize(), 0);
}

// 测试 WAL 文件路径
TEST_F(WALTest, Filepath) {
    WAL::GetInstance().Init(test_file);
    EXPECT_EQ(WAL::GetInstance().GetFilepath(), test_file);
}

// 测试数据损坏检测（CRC32 校验失败）
TEST_F(WALTest, CorruptedDataDetection) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    
    // 写入数据
    WAL::GetInstance().Init(test_file, config);
    WAL::GetInstance().AppendSet("name", "Alice");
    
    // 手动损坏文件
    {
        std::fstream file(test_file, std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(5);
        char garbage[] = "XXXXX";
        file.write(garbage, 5);
        file.close();
    }
    
    // 重新打开并读取
    WAL::GetInstance().Init(test_file);
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    
    // 损坏的记录应该被跳过
    EXPECT_EQ(records.size(), 0);
}

// 测试多条记录的追加和读取
TEST_F(WALTest, MultipleRecords) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    WAL::GetInstance().Init(test_file, config);
    
    // 追加 100 条记录
    for (int i = 0; i < 100; ++i) {
        WAL::GetInstance().AppendSet("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    // 读取所有记录
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    ASSERT_EQ(records.size(), 100);
    
    // 验证每条记录
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(records[i].op, OpType::SET);
        EXPECT_EQ(records[i].key, "key" + std::to_string(i));
        EXPECT_EQ(records[i].value, "value" + std::to_string(i));
    }
}

// 测试空值
TEST_F(WALTest, EmptyValue) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    WAL::GetInstance().Init(test_file, config);
    
    WAL::GetInstance().AppendSet("key", "");
    
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].key, "key");
    EXPECT_EQ(records[0].value, "");
}

// 测试 WAL 降级策略（文件打开失败时禁用 WAL）
TEST_F(WALTest, DegradationStrategy) {
    // 尝试打开一个无效路径的文件（路径不存在）
    std::string invalid_path = "/nonexistent/path/wal.log";
    WAL::GetInstance().Init(invalid_path);
    
    // WAL 应该被禁用，而不是抛出异常
    EXPECT_FALSE(WAL::GetInstance().IsEnabled());
    
    // 调用 Append 不应该崩溃
    WAL::GetInstance().AppendSet("key", "value");
    WAL::GetInstance().AppendDel("key");
    
    // ReadAll 应该返回空
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    EXPECT_TRUE(records.empty());
    
    // Clear 不应该崩溃
    WAL::GetInstance().Clear();
    
    // GetFileSize 应该返回 0
    EXPECT_EQ(WAL::GetInstance().GetFileSize(), 0);
}

// 测试 WAL 启用状态
TEST_F(WALTest, EnabledStatus) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    WAL::GetInstance().Init(test_file, config);
    
    // WAL 应该启用
    EXPECT_TRUE(WAL::GetInstance().IsEnabled());
    
    // 追加记录应该正常工作
    WAL::GetInstance().AppendSet("key", "value");
    
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    EXPECT_EQ(records.size(), 1);
}

// 测试 Sync() 真正写入磁盘
TEST_F(WALTest, SyncToDisk) {
    // 第一次打开：写入数据
    {
        WALConfig config;
        config.policy = FsyncPolicy::ALWAYS;
        WAL::GetInstance().Init(test_file, config);
        WAL::GetInstance().AppendSet("persistent", "data");
    }
    
    // 第二次打开：验证数据持久化
    {
        WAL::GetInstance().Init(test_file);
        std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
        
        // 数据应该还在（因为已经 fsync 到磁盘）
        ASSERT_EQ(records.size(), 1);
        EXPECT_EQ(records[0].op, OpType::SET);
        EXPECT_EQ(records[0].key, "persistent");
        EXPECT_EQ(records[0].value, "data");
    }
}

// 测试不调用 Sync() 时数据可能丢失（模拟断电）
TEST_F(WALTest, NoSyncDataLoss) {
    // 第一次打开：写入数据但不 Sync
    {
        WALConfig config;
        config.policy = FsyncPolicy::NO;
        WAL::GetInstance().Init(test_file, config);
        WAL::GetInstance().AppendSet("temporary", "data");
        // 没有调用 Sync()！
    }
    
    // 第二次打开：数据可能丢失
    {
        WAL::GetInstance().Init(test_file);
        std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
        
        // 不调用 Sync，数据可能不在磁盘上
        EXPECT_TRUE(records.empty() || records.size() == 1);
    }
}

// 测试多条记录的磁盘持久化
TEST_F(WALTest, MultipleRecordsPersistence) {
    // 写入多条记录
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    WAL::GetInstance().Init(test_file, config);
    
    for (int i = 0; i < 10; ++i) {
        WAL::GetInstance().AppendSet("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    // 验证所有记录都持久化了
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    
    ASSERT_EQ(records.size(), 10);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(records[i].op, OpType::SET);
        EXPECT_EQ(records[i].key, "key" + std::to_string(i));
        EXPECT_EQ(records[i].value, "value" + std::to_string(i));
    }
}

// ============================================================
// 刷盘策略测试
// ============================================================

// 测试 ALWAYS 策略：每条命令都 fsync
TEST_F(WALTest, AlwaysPolicy) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    WAL::GetInstance().Init(test_file, config);
    EXPECT_EQ(WAL::GetInstance().GetPolicy(), FsyncPolicy::ALWAYS);
    
    // 写入数据（每次 Append 都会 fsync）
    WAL::GetInstance().AppendSet("key1", "value1");
    WAL::GetInstance().AppendSet("key2", "value2");
    WAL::GetInstance().AppendDel("key1");
    
    // 立即读取验证
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    ASSERT_EQ(records.size(), 3);
}

// 测试 EVERYSEC 策略：每秒 fsync
TEST_F(WALTest, EverySecPolicy) {
    WALConfig config;
    config.policy = FsyncPolicy::EVERYSEC;
    config.sync_interval_ms = 50;  // 测试用 50ms
    WAL::GetInstance().Init(test_file, config);
    EXPECT_EQ(WAL::GetInstance().GetPolicy(), FsyncPolicy::EVERYSEC);
    
    // 写入数据
    WAL::GetInstance().AppendSet("key1", "value1");
    WAL::GetInstance().AppendSet("key2", "value2");
    
    // 等待后台线程刷盘
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证数据已持久化
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    EXPECT_EQ(records.size(), 2);
}

// 测试 NO 策略：不主动 fsync
TEST_F(WALTest, NoPolicy) {
    WALConfig config;
    config.policy = FsyncPolicy::NO;
    WAL::GetInstance().Init(test_file, config);
    EXPECT_EQ(WAL::GetInstance().GetPolicy(), FsyncPolicy::NO);
    
    // 写入数据
    WAL::GetInstance().AppendSet("key1", "value1");
    WAL::GetInstance().AppendSet("key2", "value2");
    
    // 数据在缓冲区，还没 fsync
}

// 测试默认策略是 EVERYSEC
TEST_F(WALTest, DefaultPolicy) {
    WAL::GetInstance().Init(test_file);
    EXPECT_EQ(WAL::GetInstance().GetPolicy(), FsyncPolicy::EVERYSEC);
}

// 测试 ALWAYS 策略的磁盘持久化
TEST_F(WALTest, AlwaysPolicyPersistence) {
    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;
    
    // 写入数据
    WAL::GetInstance().Init(test_file, config);
    WAL::GetInstance().AppendSet("persistent", "always");
    
    // 验证数据已持久化
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].value, "always");
}

// 测试 EVERYSEC 策略的批量写入
TEST_F(WALTest, EverySecBatchWrite) {
    WALConfig config;
    config.policy = FsyncPolicy::EVERYSEC;
    config.sync_interval_ms = 50;
    WAL::GetInstance().Init(test_file, config);
    
    // 快速写入 100 条
    for (int i = 0; i < 100; ++i) {
        WAL::GetInstance().AppendSet("batch" + std::to_string(i), "value" + std::to_string(i));
    }
    
    // 等待后台线程刷盘
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证所有数据
    std::vector<WALRecord> records = WAL::GetInstance().ReadAll();
    EXPECT_EQ(records.size(), 100);
}
