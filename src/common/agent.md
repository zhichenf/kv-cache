# common — 公共模块

## 职责

提供所有模块共享的基础设施：网络协议（RESP）、日志系统、持久化（WAL + Snapshot）、数据校验（CRC32）。

## 文件

| 文件 | 职责 |
|------|------|
| `protocol.h` / `protocol.cpp` | RESP 协议解析与序列化，RespReader 流式缓冲读取 |
| `logger.h` | spdlog 异步日志封装，LOG 宏定义（头文件仅） |
| `wal.h` / `wal.cpp` | WAL 预写日志（单例），支持 ALWAYS/EVERYSEC/NO 三种刷盘策略 |
| `snapshot.h` / `snapshot.cpp` | Snapshot 快照管理（单例），原子写入（.tmp + rename） |
| `crc32.h` / `crc32.cpp` | CRC32 校验算法，编译期预计算查找表 |

## 关键设计

### WAL

- **单例模式**: 全局共享一个 WAL 实例
- **降级策略**: 文件打开失败时禁用 WAL 而非崩溃
- **后台线程**: EVERYSEC 策略启动独立刷盘线程
- **原子清空**: `ClearWithoutLock()` 供快照操作使用（不持锁）
- **记录格式**: `CRC32(4B) | OpType(1B) | KeyLen(2B) | Key | ValLen(2B) | Value`

### Snapshot

- **单例模式**: 全局共享一个 Snapshot 实例
- **原子写入**: 先写 `.tmp` 临时文件 → `rename()` 原子替换
- **文件格式**: Magic(5B "KVSNP") + Version(4B) + EntryCount(4B) + Entries... + FooterCRC(4B)
- **完整性校验**: FooterCRC → Magic → Version → 逐条 Entry CRC

### Logger

- **异步日志**: spdlog async_logger，队列深度 8192，1 个后台写入线程
- **输出目标**: rotating file sink（5文件×5MB）+ 控制台彩色输出
- **日志级别**: DEBUG → INFO → WARN → ERR

### Protocol

- **RESP 格式**: 兼容 Redis 的序列化协议
- **两层错误处理**: 协议层错误（ParseError）vs 语法层错误（Response::ERR）
- **流式解析**: `RespReader` 处理 TCP 粘包/半包

### CRC32

- **编译期优化**: `constexpr` 预计算 256 项查找表
- **接口**: `AppendChecksum()` 追加校验和, `VerifyChecksum()` 验证完整性
