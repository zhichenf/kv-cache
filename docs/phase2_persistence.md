# Phase 2：持久化与崩溃恢复

## 学习目标

- 掌握文件 I/O 与顺序写（WAL）
- 理解序列化与反序列化设计
- 掌握崩溃恢复（Crash Recovery）机制
- 理解 fsync、缓冲与性能取舍

---

## 核心概念

### WAL（Write-Ahead Log）

**WAL 的核心原则**：在修改内存数据之前，先把操作写入日志文件。

```
SET key1 val1
  │
  ├─ 1. Append "SET key1 val1" → wal.log  ← 先写日志
  ├─ 2. fsync wal.log                       ← 落盘
  └─ 3. data_["key1"] = "val1"             ← 再更新内存
```

**为什么需要 WAL？**
- 如果在 3 之前 crash：重启时回放 WAL，数据恢复
- 如果在 2 之前 crash：重启时丢弃不完整记录
- WAL 保证了**操作的原子性和持久性**

### Snapshot（快照）

WAL 无限增长会带来问题：
1. 重启恢复越来越慢
2. 占用磁盘空间

**解决方案**：定期将内存数据全量 dump 到快照文件，然后截断 WAL。

```
snapshot_20260526_120000.dat  ← 时刻 T 的全量数据
wal.log                       ← T 之后的增量操作
恢复：先加载 snapshot，再回放 wal.log（远快于回放全部）
```

---

## 系统设计

### 存储目录结构

```
data/
├── wal.log               ← WAL 文件
├── snapshot.dat          ← 最新快照
└── snapshot.index        ← 快照元信息（时间戳、key 数量等）
```

### WAL 记录格式（二进制）

```
┌─────────┬──────────┬──────────┬──────────┐
│ CRC32   │ OpType   │ KeyLen   │ Key      │
│ (4字节)  │ (1字节)  │ (2字节)  │ (变长)   │
├─────────┼──────────┴──────────┴──────────┤
│ ValLen  │ Value                          │
│ (2字节)  │ (变长)                         │
└─────────┴───────────────────────────────┘
```

- OpType: 0x01=SET, 0x02=DEL
- CRC32：校验记录完整性
- 所有多字节使用小端序

### 快照格式

```
┌─────────────────────────────────┐
│ Magic: "KVSNP" (5字节)          │
│ Version: 1 (4字节)              │
│ EntryCount: N (4字节)           │
├─────────────────────────────────┤
│ KeyLen1 (2字节) │ Key1 (变长)   │
│ ValLen1 (2字节) │ Val1 (变长)   │
│ CRC1 (4字节)                    │
├─────────────────────────────────┤
│ ... (N 个 entry)               │
├─────────────────────────────────┤
│ FooterCRC (4字节，覆盖全部)     │
└─────────────────────────────────┘
```

---

## 接口变更

### 新增 PersistentKvStore

继承或包装 KvStore：

```cpp
class PersistentKvStore : public KvStore {
public:
    explicit PersistentKvStore(const std::string& data_dir);
    ~PersistentKvStore();

    void Set(const std::string& key, const std::string& value);
    bool Delete(const std::string& key);

private:
    void AppendWal(OpType op, const std::string& key,
                   const std::string& value);
    void RecoverFromWal();
    void CreateSnapshot();
    void LoadSnapshot();

    std::string data_dir_;
    std::ofstream wal_stream_;
    uint64_t last_snapshot_seq_ = 0;
    uint64_t current_seq_ = 0;
    static constexpr size_t kSnapshotInterval = 10000;  // 每 10000 次操作
};
```

### 服务端配置变更

```bash
./kv_server --port 6379 --data-dir ./data
```

---

## 实现步骤

| 序号 | 内容 | 说明 |
|------|------|------|
| 1 | WAL 追加写入 | 每次 Set/Delete 先写 WAL |
| 2 | CRC32 校验 | 判断记录是否完整 |
| 3 | 启动恢复 | RecoverFromWal 回放 |
| 4 | Snapshot 创建 | 满阈值时生成快照 |
| 5 | Snapshot 加载 | 优先加载快照再回放 WAL |
| 6 | WAL 截断 | snapshot 后清理旧 WAL |

---

## 验收标准

### 崩溃恢复

```bash
# 1. 启动并写入数据
./kv_server --port 6379 --data-dir ./data
# 写入 100 条记录，kill -9 杀掉进程

# 2. 重启
./kv_server --port 6379 --data-dir ./data
# 验证：所有写入的数据都存在
```

### Snapshot 测试

写 20000 条数据，验证 snapshot 文件生成，重启后数据完整，且恢复速度明显快于纯 WAL。

### 异常测试

- 写入 WAL 时断电：下次启动忽略不完整记录
- WAL 文件损坏（修改 CRC）：跳过损坏记录
- Snapshot 未完成时 crash：不加载不完整的 snapshot
