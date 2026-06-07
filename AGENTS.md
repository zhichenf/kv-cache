# kv_cache — 分布式 KV 存储系统

## 项目概述

基于 C++20 的分布式 KV 存储系统，采用 RESP 协议通信，支持 WAL 持久化 + Snapshot 快照恢复。

## 演进路线

| 阶段 | 状态 | 说明 |
|------|------|------|
| Phase 1 | ✅ 完成 | 单机内存版：KvStore + TcpServer + RESP 协议 |
| Phase 2 | ✅ 完成 | 持久化：WAL（预写日志）+ Snapshot（快照） |
| Phase 3 | 📋 规划 | 主从复制：1主N从，异步/半同步复制 |
| Phase 4 | 📋 规划 | 分片：一致性哈希 + 虚拟节点 |
| Phase 5 | 📋 规划 | Raft 共识：Leader 选举 + 日志复制 |
| Phase 6 | 📋 规划 | 集群管理：SWIM 故障检测 + Gossip 协议 |

## 技术栈

- **语言**: C++20
- **构建**: CMake 3.20 + MinGW (Windows) / GCC (Linux)
- **测试**: Google Test（预编译 DLL/SO）
- **性能**: Google Benchmark
- **日志**: spdlog（header-only，异步日志 + rotating file sink）
- **协议**: RESP (Redis Serialization Protocol)

## 目录结构

```
kv_cache/
├── include/                # 头文件
│   ├── common/             # 公共模块：Logger, Protocol, WAL, Snapshot, CRC32
│   ├── storage/            # 存储引擎：KvStore, PersistentKvStore
│   ├── server/             # 服务端：TcpServer, KvServer, Config
│   └── client/             # 客户端：KvClient, CliParser
├── src/                    # 源文件（与 include 对应）
│   ├── common/
│   ├── storage/
│   ├── server/
│   └── client/
├── tests/                  # 单元测试（7个测试文件，95个测试用例）
├── benchmarks/             # 性能测试（WAL 基准测试）
├── third_party/            # 第三方库：googletest, spdlog, benchmark
├── docs/                   # 设计文档：各阶段需求 + TODO
├── config.txt              # 服务端配置文件（key=value 格式）
├── CMakeLists.txt          # CMake 构建配置
├── Makefile                # 便捷构建命令
└── AGENTS.md               # 本文件
```

## 构建命令

```bash
make build          # 编译所有目标
make test_all       # 运行所有测试（95个）
make server         # 启动服务端
make client ARGS="-h 127.0.0.1 -p 6379"  # 启动客户端
make bench          # 运行性能测试
make clean          # 清理构建产物
```

## 架构设计

```
Client ──RESP──> TcpServer ──callback──> KvServer ──> PersistentKvStore ──> KvStore (内存)
                                          │                │
                                          │                ├── WAL (持久化日志)
                                          │                └── Snapshot (快照)
                                          ├── Protocol (RESP 解析)
                                          └── Logger (异步日志)
```

## 核心设计模式

| 模式 | 应用 |
|------|------|
| 单例模式 | Logger, WAL, Snapshot（Meyer's singleton） |
| 读写锁 | KvStore（shared_mutex 读写分离） |
| 回调模式 | TcpServer::OnMessageCallback |
| 继承多态 | PersistentKvStore : KvStore（重写 Set/Delete） |
| 原子操作 | PersistentKvStore::op_count_（atomic） |
| 降级容错 | WAL 文件打开失败时禁用而非崩溃 |
| 原子写入 | Snapshot 先写 .tmp 再 rename |
| RAII | KvClient（析构断开连接）、WAL（析构关闭） |

## 关键参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `kSnapshotInterval` | 500 | 操作数达到此值触发快照 |
| `kBurstMaxOps` | 100 | 突发窗口内最大操作数 |
| `kBurstWindowMs` | 1000 | 突发检测窗口（毫秒） |
| WAL 刷盘策略 | NO | ALWAYS/EVERYSEC/NO |
| 日志轮转 | 5文件×5MB | spdlog rotating file sink |

## 配置文件

`config.txt` 格式（key=value）：

```
port=6379
data_dir=./data
log_level=info
log_dir=logs
log_max_files=5
log_max_size_mb=5
```

CLI 参数优先级高于配置文件：`kv_server -p 8080 -d /data -l debug`

## 数据目录结构

```
data/
├── wal.log          # WAL 预写日志
└── snapshot.dat     # 快照文件
logs/
└── kv_cache.log     # 异步日志（轮转）
```

## 代码规范

1. 每个函数前注释说明职责
2. `if` 必须用 `{}` 包裹
3. 命名表明清楚含义
4. 仅本文件使用的工具函数放匿名 namespace
5. 跨平台代码用 `#ifdef _WIN32` 包裹在辅助函数内部
6. 头文件放 `include/`，源文件放 `src/`，目录结构对应
