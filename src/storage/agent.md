# storage — 存储引擎

## 职责

提供线程安全的内存 KV 存储，以及基于 WAL + Snapshot 的持久化存储。

## 文件

| 文件 | 职责 |
|------|------|
| `kv_store.h` / `kv_store.cpp` | 线程安全内存 KV 存储（shared_mutex 读写分离） |
| `persistent_kv_store.h` / `persistent_kv_store.cpp` | 持久化 KV 存储（继承 KvStore，WAL + Snapshot） |

## 关键设计

### KvStore

- **数据结构**: `std::unordered_map<std::string, std::string>`
- **线程安全**: `std::shared_mutex`（读写分离）
  - 读操作（Get/Exists/AllKeys/Size）: `shared_lock`
  - 写操作（Set/Delete）: `unique_lock`
- **成员**: `protected` 访问级别，允许 PersistentKvStore 继承

### PersistentKvStore

- **继承**: `PersistentKvStore : KvStore`（重写 Set/Delete）
- **组合**: 持有 WAL 和 Snapshot 的引用
- **原子计数器**: `std::atomic<uint64_t> op_count_`（线程安全）
- **滑动窗口**: `SlidingWindow` 检测突发写入

#### 写入流程（Set/Delete）

```
1. 写 WAL（持久化日志）
2. 更新内存（KvStore 写锁）
3. 原子计数器++ 
4. 滑动窗口检测
5. 检查快照触发条件
```

#### 崩溃恢复（Recover）

```
1. 加载 Snapshot（如果有）
2. 回放 WAL（从 Snapshot 之后的日志）
```

#### 快照触发条件

- 总操作数 >= `kSnapshotInterval`（默认 500）
- 突发窗口超限（1秒内 > `kBurstMaxOps`=100 次）

#### 原子快照操作

```
锁 WAL mutex → 锁 KvStore 写锁 → 获取数据 → 创建快照 → 清空 WAL → 重置计数器 → 释放锁
```

### SlidingWindow

- **数据结构**: `std::deque<time_point>` 存储时间戳
- **功能**: 检测指定时间窗口内的操作次数是否超限
- **线程安全**: `std::mutex` 保护
- **窗口**: 默认 1 秒（`kBurstWindowMs`=1000ms）
