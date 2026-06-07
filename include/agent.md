# include — 头文件目录

## 职责

定义所有模块的公共接口，与 `src/` 目录一一对应。

## 目录结构

```
include/
├── common/             # 公共基础设施
│   ├── logger.h        # 异步日志系统（spdlog 封装）
│   ├── protocol.h      # RESP 协议解析与序列化
│   ├── wal.h           # WAL 预写日志（单例）
│   ├── snapshot.h      # Snapshot 快照管理（单例）
│   └── crc32.h         # CRC32 校验算法
├── storage/            # 存储引擎
│   ├── kv_store.h      # 线程安全内存 KV 存储
│   └── persistent_kv_store.h  # 持久化 KV 存储（WAL + Snapshot）
├── server/             # 服务端
│   ├── tcp_server.h    # TCP 网络层（纯收发字节流）
│   ├── kv_server.h     # 服务端编排类（整合所有组件）
│   └── config.h        # 配置文件解析
└── client/             # 客户端
    ├── client.h        # RESP 协议封装的 TCP 客户端
    └── cli_parser.h    # 用户文本命令解析器
```

## 文件详情

### common/logger.h

- **类**: `Logger`（单例）
- **枚举**: `LogLevel {DEBUG, INFO, WARN, ERR}`
- **功能**: 基于 spdlog 的异步日志，支持文件（rotating file sink）+ 控制台输出
- **关键函数**: `Init()`, `Flush()`, `Shutdown()`, `SetLevel()`
- **宏**: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`

### common/protocol.h

- **枚举**: `CommandType {SET, GET, DEL, EXISTS, KEYS, UNKNOWN}`
- **枚举**: `ParseError {NONE, EMPTY, UNKNOWN_COMMAND, WRONG_ARG_COUNT, PROTOCOL_ERROR}`
- **结构体**: `Command`, `ParseResult`, `Response`
- **类**: `RespReader` — TCP 流式缓冲读取器，处理粘包/半包
- **函数**: `ParseCommand()`, `SerializeResponse()`, `SerializeCommand()`

### common/wal.h

- **类**: `WAL`（单例，RAII）
- **枚举**: `OpType {SET=1, DEL=2}`, `FsyncPolicy {ALWAYS, EVERYSEC, NO}`
- **结构体**: `WALConfig`, `WALRecord`
- **线程安全**: `std::mutex` 保护所有操作
- **后台线程**: `std::thread sync_thread_` 用于 EVERYSEC 刷盘
- **关键函数**: `Init()`, `Shutdown()`, `Append()`, `ReadAll()`, `Clear()`, `GetMutex()`

### common/snapshot.h

- **类**: `Snapshot`（单例，RAII）
- **结构体**: `SnapshotHeader`（Magic + Version + EntryCount）, `SnapshotEntry`
- **关键函数**: `Create()`, `Load()`, `IsValid()`, `Remove()`
- **文件格式**: Magic(5B) + Version(4B) + EntryCount(4B) + Entries + FooterCRC(4B)

### common/crc32.h

- **命名空间**: `crc`
- **函数**: `CRC32()`（三个重载）, `AppendChecksum()`, `ReadChecksum()`, `VerifyChecksum()`
- **实现**: 编译期预计算查找表（`constexpr`）

### storage/kv_store.h

- **类**: `KvStore` — 线程安全内存 KV 存储
- **数据结构**: `std::unordered_map<std::string, std::string>`
- **线程安全**: `std::shared_mutex`（读写分离）
- **接口**: `Set()`, `Get()`, `Delete()`, `Exists()`, `AllKeys()`, `Size()`
- **成员**: `protected` 访问级别，允许 PersistentKvStore 继承

### storage/persistent_kv_store.h

- **类**: `PersistentKvStore : KvStore` — 持久化 KV 存储
- **组合**: 持有 WAL 和 Snapshot 的引用
- **原子计数器**: `std::atomic<uint64_t> op_count_`
- **重写**: `Set()`, `Delete()` — 先写 WAL 再更新内存
- **关键函数**: `Recover()`, `CreateSnapshot()`, `ForceSnapshot()`
- **辅助类**: `SlidingWindow` — 滑动窗口检测突发写入

### server/tcp_server.h

- **类**: `TcpServer` — 纯 TCP 网络层
- **回调**: `OnMessageCallback = std::function<string(int, const string&)>`
- **线程模型**: accept 线程 + 每连接一个处理线程（detach）
- **跨平台**: WinSock2 / POSIX socket

### server/kv_server.h

- **类**: `KvServer` — 服务端核心编排类
- **组合**: PersistentKvStore + TcpServer + RespReader map
- **每连接状态**: `unordered_map<int, RespReader>` 处理粘包
- **关键函数**: `Start()`, `Stop()`, `SetLogLevel()`

### server/config.h

- **结构体**: `ServerConfig`
- **字段**: port, data_dir, log_level, log_dir, log_max_files, log_max_size_mb
- **函数**: `Load()`（从文件）, `Save()`, `OverrideFromArgs()`（CLI 覆盖）

### client/client.h

- **类**: `KvClient` — RESP 协议封装的 TCP 客户端
- **RAII**: 析构自动断开连接
- **接口**: `Connect()`, `Disconnect()`, `Set()`, `Get()`, `Delete()`, `Exists()`, `Keys()`

### client/cli_parser.h

- **类**: `CliParser` — 用户文本命令解析器
- **函数**: `Parse(const string&)` → `optional<Command>`
