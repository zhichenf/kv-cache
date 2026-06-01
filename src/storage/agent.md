# storage — 存储引擎

## 职责

提供线程安全的内存 KV 存储，基于 `unordered_map` + `shared_mutex` 实现读写分离。

## 文件

| 文件 | 职责 |
|------|------|
| `kv_store.h` | 导出 KvStore 类声明 |
| `kv_store.cpp` | 实现 Set/Get/Delete/Exists/AllKeys/Size 操作 |
