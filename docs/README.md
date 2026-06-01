# 分布式 KV Cache — 从零搭建教学计划

## 总目标

从单机内存版逐步演进到分布式 KV 存储，理解分布式系统核心原理（一致性、分片、复制、共识），同时掌握现代 C++ 编程实践。

---

## 六阶段教学路线

```
Phase 1 ─────── Phase 2 ─────── Phase 3 ─────── Phase 4 ─────── Phase 5 ─────── Phase 6
  单机内存版       持久化            主从复制         分片              共识算法        集群管理
                                         │
                                    ┌─────┴─────┐
                                    │            │
                              同步复制        异步复制
```

---

### Phase 1：单机内存版 KV Store
> **目标**：构建一个基于 TCP 协议的单机键值存储服务

- 知识点：C++ 多线程（`std::shared_mutex`）、TCP socket 编程、文本协议设计
- 产出：`kv_server` + `kv_client` CLI 工具
- 关键设计：类 Redis 文本协议、线程安全存储引擎

### Phase 2：持久化与崩溃恢复
> **目标**：为 KV Store 添加 WAL（Write-Ahead Log）和快照能力

- 知识点：文件 I/O、序列化/反序列化、崩溃恢复、fsync
- 产出：宕机后数据不丢失
- 关键设计：WAL + Snapshot 双机制，分段清理

### Phase 3：主从复制
> **目标**：实现 1 主 N 从的异步/半同步复制

- 知识点：Replication、raft 前奏、CAP 感性认识、一致性模型
- 产出：主节点写入，从节点查询，故障手动切换
- 关键设计：Replication Log、heartbeat、offset 追踪

### Phase 4：分片（Sharding）
> **目标**：用一致性哈希将数据分布到多个节点

- 知识点：一致性哈希、虚拟节点、请求路由、扩缩容
- 产出：多节点自动路由，增删节点影响最小
- 关键设计：Ketama hash、虚拟节点 160 个、路由表动态更新

### Phase 5：Raft 共识算法
> **目标**：实现 Raft 共识，保证多节点强一致性

- 知识点：Leader 选举、日志复制（Log Replication）、安全保证
- 产出：完整的 Raft 共识层
- 关键设计：Term & Election timeout、AppendEntries、majority

### Phase 6：集群管理与运维
> **目标**：自动化集群管理、故障检测与恢复

- 知识点：Gossip 协议、故障检测、自动 rebalance
- 产出：可运维的分布式 KV 系统
- 关键设计：SWIM 故障检测、自动迁移、配置中心

---

## 技术栈

| 层面 | 选择 | 原因 |
|------|------|------|
| 语言 | C++20 | 现代 C++ 特性（smart ptr、optional、coroutines 等）|
| 网络 | POSIX socket / WinSock2 | 跨平台，理解底层 |
| 并发 | `std::thread` + `std::shared_mutex` | C++ 标准库，无外部依赖 |
| 构建 | CMake 3.20+ | 业界标准 |
| 序列化 | 自定义二进制（后续可选 flatbuffers） | 学习目的 |

## 每阶段交付物格式

每个阶段包含 4 个文件：
```
docs/phase<N>_<name>.md    ← 需求文档（概念说明 + 接口定义 + 验收标准）
src/...                     ← 实现代码
tests/...                   ← 测试
```

---

下一阶段：[Phase 1 - 单机内存版 KV Store](phase1_in_memory_kv.md)
