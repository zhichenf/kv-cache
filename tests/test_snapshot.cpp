#include <gtest/gtest.h>
#include "common/snapshot.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

namespace fs = std::filesystem;

class SnapshotTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = (fs::temp_directory_path() / "kv_cache_snapshot_test").string();
        fs::create_directories(test_dir_);
    }
    
    void TearDown() override {
        fs::remove_all(test_dir_);
    }
    
    std::string test_dir_;
};

// ============================================================
// 基本功能测试
// ============================================================

TEST_F(SnapshotTest, CreateAndLoad) {
    auto& snapshot = Snapshot::GetInstance();
    
    // 创建测试数据
    std::vector<std::pair<std::string, std::string>> entries = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"}
    };
    
    // 创建快照
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    // 验证快照文件存在
    EXPECT_TRUE(snapshot.IsValid(test_dir_));
    
    // 加载快照
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_TRUE(snapshot.Load(test_dir_, loaded_entries));
    
    // 验证数据
    ASSERT_EQ(loaded_entries.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        EXPECT_EQ(loaded_entries[i].first, entries[i].first);
        EXPECT_EQ(loaded_entries[i].second, entries[i].second);
    }
}

TEST_F(SnapshotTest, EmptySnapshot) {
    auto& snapshot = Snapshot::GetInstance();
    
    // 创建空快照
    std::vector<std::pair<std::string, std::string>> entries;
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    // 加载空快照
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_TRUE(snapshot.Load(test_dir_, loaded_entries));
    EXPECT_EQ(loaded_entries.size(), 0);
}

TEST_F(SnapshotTest, LargeValues) {
    auto& snapshot = Snapshot::GetInstance();
    
    // 创建大 value 的测试数据
    std::string large_value(10000, 'x');  // 10KB value
    std::vector<std::pair<std::string, std::string>> entries = {
        {"small_key", "small_value"},
        {"large_key", large_value}
    };
    
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_TRUE(snapshot.Load(test_dir_, loaded_entries));
    
    ASSERT_EQ(loaded_entries.size(), 2);
    EXPECT_EQ(loaded_entries[0].second, "small_value");
    EXPECT_EQ(loaded_entries[1].second, large_value);
}

// ============================================================
// 完整性测试
// ============================================================

TEST_F(SnapshotTest, CorruptedMagic) {
    auto& snapshot = Snapshot::GetInstance();
    
    std::vector<std::pair<std::string, std::string>> entries = {{"key", "value"}};
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    // 修改 Magic
    std::string filepath = snapshot.GetSnapshotPath(test_dir_);
    std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);
    char bad_magic[5] = {'B', 'A', 'D', '!', '#'};
    file.seekp(0);
    file.write(bad_magic, 5);
    file.close();
    
    // 加载应该失败
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_FALSE(snapshot.Load(test_dir_, loaded_entries));
    EXPECT_FALSE(snapshot.IsValid(test_dir_));
}

TEST_F(SnapshotTest, CorruptedEntryCRC) {
    auto& snapshot = Snapshot::GetInstance();
    
    std::vector<std::pair<std::string, std::string>> entries = {
        {"key1", "value1"},
        {"key2", "value2"}
    };
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    // 修改第二个 entry 的 value
    std::string filepath = snapshot.GetSnapshotPath(test_dir_);
    std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);
    
    // 跳过 header (5+4+4=13 bytes) 和第一个 entry
    // 第一个 entry: 2(key_len) + 4(key) + 2(val_len) + 6(value) + 4(crc) = 18 bytes
    file.seekg(13 + 18);
    
    // 读取第二个 entry 的 key_len
    char key_len_buf[2];
    file.read(key_len_buf, 2);
    uint16_t key_len = static_cast<uint8_t>(key_len_buf[0]) | (static_cast<uint8_t>(key_len_buf[1]) << 8);
    
    // 跳到 value 开始位置并修改
    file.seekp(13 + 18 + 2 + key_len + 2);
    char bad_value[] = "CORRUPTED";
    file.write(bad_value, strlen(bad_value));
    file.close();
    
    // 加载应该失败（CRC 不匹配）
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_FALSE(snapshot.Load(test_dir_, loaded_entries));
}

TEST_F(SnapshotTest, CorruptedFooterCRC) {
    auto& snapshot = Snapshot::GetInstance();
    
    std::vector<std::pair<std::string, std::string>> entries = {{"key", "value"}};
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    // 修改 FooterCRC
    std::string filepath = snapshot.GetSnapshotPath(test_dir_);
    std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(-4, std::ios::end);
    char bad_crc[] = {0x00, 0x00, 0x00, 0x00};
    file.write(bad_crc, 4);
    file.close();
    
    // 加载应该失败
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_FALSE(snapshot.Load(test_dir_, loaded_entries));
}

// ============================================================
// 文件操作测试
// ============================================================

TEST_F(SnapshotTest, RemoveSnapshot) {
    auto& snapshot = Snapshot::GetInstance();
    
    std::vector<std::pair<std::string, std::string>> entries = {{"key", "value"}};
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    EXPECT_TRUE(snapshot.IsValid(test_dir_));
    
    EXPECT_TRUE(snapshot.Remove(test_dir_));
    EXPECT_FALSE(snapshot.IsValid(test_dir_));
}

TEST_F(SnapshotTest, LoadNonExistent) {
    auto& snapshot = Snapshot::GetInstance();
    
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_FALSE(snapshot.Load(test_dir_, loaded_entries));
}

TEST_F(SnapshotTest, OverwriteSnapshot) {
    auto& snapshot = Snapshot::GetInstance();
    
    // 创建第一个快照
    std::vector<std::pair<std::string, std::string>> entries1 = {{"key1", "value1"}};
    EXPECT_TRUE(snapshot.Create(test_dir_, entries1));
    
    // 创建第二个快照（覆盖）
    std::vector<std::pair<std::string, std::string>> entries2 = {{"key2", "value2"}};
    EXPECT_TRUE(snapshot.Create(test_dir_, entries2));
    
    // 加载应该是第二个快照
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_TRUE(snapshot.Load(test_dir_, loaded_entries));
    
    ASSERT_EQ(loaded_entries.size(), 1);
    EXPECT_EQ(loaded_entries[0].first, "key2");
    EXPECT_EQ(loaded_entries[0].second, "value2");
}

// ============================================================
// 边界条件测试
// ============================================================

TEST_F(SnapshotTest, ManyEntries) {
    auto& snapshot = Snapshot::GetInstance();
    
    // 创建 1000 个 entry
    std::vector<std::pair<std::string, std::string>> entries;
    for (int i = 0; i < 1000; ++i) {
        entries.push_back({"key" + std::to_string(i), "value" + std::to_string(i)});
    }
    
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_TRUE(snapshot.Load(test_dir_, loaded_entries));
    
    ASSERT_EQ(loaded_entries.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        EXPECT_EQ(loaded_entries[i].first, entries[i].first);
        EXPECT_EQ(loaded_entries[i].second, entries[i].second);
    }
}

TEST_F(SnapshotTest, EmptyKeys) {
    auto& snapshot = Snapshot::GetInstance();
    
    std::vector<std::pair<std::string, std::string>> entries = {
        {"", "empty_key"},
        {"key", ""},
        {"", ""}
    };
    
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_TRUE(snapshot.Load(test_dir_, loaded_entries));
    
    ASSERT_EQ(loaded_entries.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        EXPECT_EQ(loaded_entries[i].first, entries[i].first);
        EXPECT_EQ(loaded_entries[i].second, entries[i].second);
    }
}

TEST_F(SnapshotTest, BinaryData) {
    auto& snapshot = Snapshot::GetInstance();
    
    // 创建包含二进制数据的 entry
    std::string binary_key(100, '\x00');
    std::string binary_value(200, '\xff');
    binary_key[50] = '\x80';
    binary_value[100] = '\x01';
    
    std::vector<std::pair<std::string, std::string>> entries = {
        {binary_key, binary_value}
    };
    
    EXPECT_TRUE(snapshot.Create(test_dir_, entries));
    
    std::vector<std::pair<std::string, std::string>> loaded_entries;
    EXPECT_TRUE(snapshot.Load(test_dir_, loaded_entries));
    
    ASSERT_EQ(loaded_entries.size(), 1);
    EXPECT_EQ(loaded_entries[0].first, binary_key);
    EXPECT_EQ(loaded_entries[0].second, binary_value);
}
