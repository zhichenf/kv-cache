#include <gtest/gtest.h>
#include "storage/persistent_kv_store.h"
#include "common/wal.h"
#include "common/snapshot.h"
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class PersistentKvStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = (fs::temp_directory_path() / "kv_cache_persistent_test").string();
        fs::create_directories(test_dir_);
        
        // 关闭 WAL 单例（如果之前已初始化）
        WAL::GetInstance().Shutdown();
    }
    
    void TearDown() override {
        WAL::GetInstance().Shutdown();
        fs::remove_all(test_dir_);
    }
    
    std::string test_dir_;
};

// ============================================================
// 基本功能测试
// ============================================================

TEST_F(PersistentKvStoreTest, SetAndGet) {
    PersistentKvStore store(test_dir_);
    
    store.Set("key1", "value1");
    auto result = store.Get("key1");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value1");
}

TEST_F(PersistentKvStoreTest, SetOverwrite) {
    PersistentKvStore store(test_dir_);
    
    store.Set("key1", "value1");
    store.Set("key1", "value2");
    
    auto result = store.Get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value2");
}

TEST_F(PersistentKvStoreTest, Delete) {
    PersistentKvStore store(test_dir_);
    
    store.Set("key1", "value1");
    EXPECT_TRUE(store.Delete("key1"));
    EXPECT_FALSE(store.Get("key1").has_value());
}

TEST_F(PersistentKvStoreTest, DeleteNonExistent) {
    PersistentKvStore store(test_dir_);
    
    EXPECT_FALSE(store.Delete("nonexistent"));
}

// ============================================================
// WAL 持久化测试
// ============================================================

TEST_F(PersistentKvStoreTest, WALPersistence) {
    // 写入数据
    {
        PersistentKvStore store(test_dir_);
        store.Set("key1", "value1");
        store.Set("key2", "value2");
    }
    
    // 重新打开，验证数据从 WAL 恢复
    {
        PersistentKvStore store(test_dir_);
        auto result1 = store.Get("key1");
        auto result2 = store.Get("key2");
        
        ASSERT_TRUE(result1.has_value());
        EXPECT_EQ(*result1, "value1");
        ASSERT_TRUE(result2.has_value());
        EXPECT_EQ(*result2, "value2");
    }
}

// ============================================================
// Snapshot 测试
// ============================================================

TEST_F(PersistentKvStoreTest, SnapshotTrigger) {
    PersistentKvStore store(test_dir_);
    
    // 写入超过阈值的数据
    for (int i = 0; i < 501; ++i) {
        store.Set("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    // 验证快照已创建
    EXPECT_TRUE(Snapshot::GetInstance().IsValid(test_dir_));
}

TEST_F(PersistentKvStoreTest, SnapshotPersistence) {
    // 写入数据触发快照
    {
        PersistentKvStore store(test_dir_);
        for (int i = 0; i < 501; ++i) {
            store.Set("key" + std::to_string(i), "value" + std::to_string(i));
        }
    }
    
    // 重新打开，验证数据从快照恢复
    {
        PersistentKvStore store(test_dir_);
        for (int i = 0; i < 501; ++i) {
            auto result = store.Get("key" + std::to_string(i));
            ASSERT_TRUE(result.has_value()) << "key" + std::to_string(i);
            EXPECT_EQ(*result, "value" + std::to_string(i));
        }
    }
}

// ============================================================
// SlidingWindow 测试
// ============================================================

TEST_F(PersistentKvStoreTest, SlidingWindow) {
    SlidingWindow window(3, std::chrono::milliseconds(100));
    
    EXPECT_FALSE(window.IsExceeded());
    
    window.AddOp();
    window.AddOp();
    window.AddOp();
    EXPECT_FALSE(window.IsExceeded());  // 刚好 3 次，不超过
    
    window.AddOp();
    EXPECT_TRUE(window.IsExceeded());   // 4 次，超过阈值 3
}

TEST_F(PersistentKvStoreTest, SlidingWindowExpire) {
    SlidingWindow window(1, std::chrono::milliseconds(100));
    
    window.AddOp();
    window.AddOp();
    EXPECT_TRUE(window.IsExceeded());  // 2 次，超过阈值 1
    
    // 等待窗口过期
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    EXPECT_FALSE(window.IsExceeded());  // 过期后重新计数
}

// ============================================================
// ForceSnapshot 测试
// ============================================================

TEST_F(PersistentKvStoreTest, ForceSnapshot) {
    PersistentKvStore store(test_dir_);
    
    store.Set("key1", "value1");
    store.Set("key2", "value2");
    
    // 手动触发快照
    store.ForceSnapshot();
    
    // 验证快照已创建
    EXPECT_TRUE(Snapshot::GetInstance().IsValid(test_dir_));
    
    // 验证数据仍然存在
    auto result1 = store.Get("key1");
    auto result2 = store.Get("key2");
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
}

// ============================================================
// 并发测试
// ============================================================

TEST_F(PersistentKvStoreTest, ConcurrentWrites) {
    PersistentKvStore store(test_dir_);
    
    // 多线程并发写入
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < 100; ++i) {
                std::string key = "key" + std::to_string(t * 100 + i);
                std::string value = "value" + std::to_string(t * 100 + i);
                store.Set(key, value);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有数据都存在
    for (int i = 0; i < 400; ++i) {
        auto result = store.Get("key" + std::to_string(i));
        ASSERT_TRUE(result.has_value()) << "key" + std::to_string(i);
    }
}

TEST_F(PersistentKvStoreTest, ConcurrentReadWrite) {
    PersistentKvStore store(test_dir_);
    
    // 预先写入一些数据
    for (int i = 0; i < 100; ++i) {
        store.Set("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    // 多线程并发读写
    std::vector<std::thread> threads;
    
    // 写线程
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < 100; ++i) {
                std::string key = "wkey" + std::to_string(t * 100 + i);
                std::string value = "wvalue" + std::to_string(t * 100 + i);
                store.Set(key, value);
            }
        });
    }
    
    // 读线程
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < 100; ++i) {
                std::string key = "key" + std::to_string(i);
                store.Get(key);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有写入的数据都存在
    for (int i = 0; i < 400; ++i) {
        auto result = store.Get("wkey" + std::to_string(i));
        ASSERT_TRUE(result.has_value()) << "wkey" + std::to_string(i);
    }
}

TEST_F(PersistentKvStoreTest, ConcurrentWriteDelete) {
    PersistentKvStore store(test_dir_);
    
    // 先写入一批数据
    for (int i = 0; i < 100; ++i) {
        store.Set("old" + std::to_string(i), "value" + std::to_string(i));
    }
    
    // 多线程并发：删除旧数据 + 写入新数据（不重叠的 key 范围）
    std::vector<std::thread> threads;
    
    // 删除线程：删除旧数据
    threads.emplace_back([&store]() {
        for (int i = 0; i < 100; ++i) {
            store.Delete("old" + std::to_string(i));
        }
    });
    
    // 写线程 1：写入 new 数据
    threads.emplace_back([&store]() {
        for (int i = 0; i < 100; ++i) {
            store.Set("new" + std::to_string(i), "value" + std::to_string(i));
        }
    });
    
    // 写线程 2：写入更多数据
    threads.emplace_back([&store]() {
        for (int i = 0; i < 100; ++i) {
            store.Set("more" + std::to_string(i), "value" + std::to_string(i));
        }
    });
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证：旧数据全部删除
    for (int i = 0; i < 100; ++i) {
        EXPECT_FALSE(store.Get("old" + std::to_string(i)).has_value())
            << "old" + std::to_string(i) << " should be deleted";
    }
    // 验证：新数据全部存在
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(store.Get("new" + std::to_string(i)).has_value())
            << "new" + std::to_string(i) << " should exist";
        EXPECT_TRUE(store.Get("more" + std::to_string(i)).has_value())
            << "more" + std::to_string(i) << " should exist";
    }
}

TEST_F(PersistentKvStoreTest, ConcurrentSnapshot) {
    PersistentKvStore store(test_dir_);
    
    // 多线程并发写入，触发快照
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < 200; ++i) {
                std::string key = "key" + std::to_string(t * 200 + i);
                std::string value = "value" + std::to_string(t * 200 + i);
                store.Set(key, value);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证快照已创建
    EXPECT_TRUE(Snapshot::GetInstance().IsValid(test_dir_));
    
    // 验证所有数据都存在
    for (int i = 0; i < 800; ++i) {
        auto result = store.Get("key" + std::to_string(i));
        ASSERT_TRUE(result.has_value()) << "key" + std::to_string(i);
    }
}

// ============================================================
// Delete 持久化测试
// ============================================================

TEST_F(PersistentKvStoreTest, DeletePersistence) {
    // 写入数据
    {
        PersistentKvStore store(test_dir_);
        store.Set("key1", "value1");
        store.Set("key2", "value2");
        store.Delete("key1");
    }
    
    // 重新打开，验证删除已持久化
    {
        PersistentKvStore store(test_dir_);
        EXPECT_FALSE(store.Get("key1").has_value());
        auto result2 = store.Get("key2");
        ASSERT_TRUE(result2.has_value());
        EXPECT_EQ(*result2, "value2");
    }
}
