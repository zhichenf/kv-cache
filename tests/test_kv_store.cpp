#include <gtest/gtest.h>
#include "storage/kv_store.h"

// Set 写入后 Get 能正确读取
TEST(KvStoreTest, SetAndGet) {
    KvStore store;
    store.Set("name", "alice");
    auto val = store.Get("name");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "alice");
}

// Get 不存在的 key 返回 nullopt
TEST(KvStoreTest, GetNotFound) {
    KvStore store;
    auto val = store.Get("nonexistent");
    EXPECT_FALSE(val.has_value());
}

// Set 覆盖已存在的 key
TEST(KvStoreTest, SetOverwrite) {
    KvStore store;
    store.Set("key", "value1");
    store.Set("key", "value2");
    auto val = store.Get("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "value2");
}

// Delete 删除存在的 key 返回 true
TEST(KvStoreTest, DeleteExisting) {
    KvStore store;
    store.Set("key", "value");
    EXPECT_TRUE(store.Delete("key"));
    EXPECT_FALSE(store.Get("key").has_value());
}

// Delete 删除不存在的 key 返回 false
TEST(KvStoreTest, DeleteNotFound) {
    KvStore store;
    EXPECT_FALSE(store.Delete("nonexistent"));
}

// Exists 检查存在的 key 返回 true
TEST(KvStoreTest, ExistsTrue) {
    KvStore store;
    store.Set("key", "value");
    EXPECT_TRUE(store.Exists("key"));
}

// Exists 检查不存在的 key 返回 false
TEST(KvStoreTest, ExistsFalse) {
    KvStore store;
    EXPECT_FALSE(store.Exists("nonexistent"));
}

// AllKeys 返回所有 key
TEST(KvStoreTest, AllKeys) {
    KvStore store;
    store.Set("a", "1");
    store.Set("b", "2");
    store.Set("c", "3");
    auto keys = store.AllKeys();
    EXPECT_EQ(keys.size(), 3u);
}

// 空存储时 AllKeys 返回空
TEST(KvStoreTest, AllKeysEmpty) {
    KvStore store;
    auto keys = store.AllKeys();
    EXPECT_TRUE(keys.empty());
}

// Size 返回正确数量
TEST(KvStoreTest, Size) {
    KvStore store;
    EXPECT_EQ(store.Size(), 0u);
    store.Set("a", "1");
    EXPECT_EQ(store.Size(), 1u);
    store.Set("b", "2");
    EXPECT_EQ(store.Size(), 2u);
}

// 删除后 Size 正确递减
TEST(KvStoreTest, SizeAfterDelete) {
    KvStore store;
    store.Set("a", "1");
    store.Set("b", "2");
    EXPECT_EQ(store.Size(), 2u);
    store.Delete("a");
    EXPECT_EQ(store.Size(), 1u);
}

// 多线程并发读写不崩溃
TEST(KvStoreTest, ConcurrentReadWrite) {
    KvStore store;
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&store, i]() {
            std::string key = "key" + std::to_string(i);
            store.Set(key, std::to_string(i));
            store.Get(key);
            store.Exists(key);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(store.Size(), 10u);
}
