# Phase 5：Raft 共识算法

## 学习目标

- 理解分布式共识的核心原理
- 掌握 Raft 的三个子问题：Leader 选举、日志复制、安全性
- 理解 Term、Election Timeout、Quorum 等核心概念
- 实现一个生产可用的 Raft 层

---

## 核心概念

### 为什么需要 Raft？

Phase 3 的主从复制有严重问题：

```
写入 Master → 异步复制 → Slave
         ↓
如果 Master 在复制前 crash：
- Client 认为写入成功
- 新选举的 Master（原 Slave）丢失了这条数据
- **数据不一致！**
```

**Raft 解决方式**：写入必须在**大多数节点（Quorum）**都确认后才返回成功。

### Raft 角色

| 角色 | 行为 |
|------|------|
| **Leader** | 处理客户端请求，管理日志复制，正常时唯一 |
| **Follower** | 被动接受 Leader 的日志，投票 |
| **Candidate** | 选举阶段的临时角色 |

### Raft 核心机制

```
Term（任期）：
Term 1      Term 2      Term 3
  │           │           │
  ▼           ▼           ▼
──●─────●─────●─────●─────●──→ time
Leader      Leader      (无 Leader → 选举)

选举过程（Election）：
Follower → Election Timeout → Candidate
  │                              │
  │   RequestVote RPC ──────────▶│
  │   ◀──── Vote Granted ────────│
  │           ↓                  │
  │   收到多数投票 → Leader      │
```

### 日志复制

```
Client → Leader → (Log Entry)
                  │
                  ├─ AppendEntries RPC ──▶ Follower 1
                  ├─ AppendEntries RPC ──▶ Follower 2
                  │
                  ◀──── Ack (已持久化) ───
                  │
                  ↓ 收到多数 ack
                  → apply to state machine → 返回 Client
```

---

## 系统设计

### Raft 层架构

```
┌─────────────────────────────┐
│     State Machine (KvStore)  │  ← 状态机
├─────────────────────────────┤
│         Raft Layer           │  ← 共识层
│  ┌──────────┬────────────┐  │
│  │ Log      │ Consensus  │  │
│  │ []Entry  │ Module     │  │
│  └──────────┴────────────┘  │
├─────────────────────────────┤
│         Transport            │  ← 网络层（gRPC/TCP）
└─────────────────────────────┘
```

### Raft 日志条目

```cpp
struct LogEntry {
    uint64_t term;          // 创建时的 term
    uint64_t index;         // 日志序号（单调递增）
    enum OpType { SET, DEL };
    OpType op;
    std::string key;
    std::string value;
};
```

### RPC 定义

```
RequestVote RPC:
  Request:  term, candidateId, lastLogIndex, lastLogTerm
  Response: term, voteGranted

AppendEntries RPC:
  Request:  term, leaderId, prevLogIndex, prevLogTerm,
            entries[], leaderCommit
  Response: term, success, lastLogIndex
```

### 持久化

Raft 需要持久化 3 个状态：
```
currentTerm   → 当前任期（重启后继续）
votedFor      → 投票给谁（防止重复投票）
log[]         → 日志条目（保证持久性）
```

使用 Phase 2 类似的方式持久化到文件。

---

## 实现步骤

| 步骤 | 内容 | 说明 |
|------|------|------|
| 1 | Raft 状态机 | Node 状态、Term、日志存储 |
| 2 | Leader 选举 | Election Timeout、RequestVote |
| 3 | 日志复制 | AppendEntries、心跳 |
| 4 | 提交与 Apply | majority 确认 → 提交 → apply 到 state machine |
| 5 | 安全性 | 选举限制（只能选包含所有已提交日志的节点）|
| 6 | 集群变更 | 成员变更（Joint Consensus）|

---

## 验收标准

### 基本共识

```bash
# 启动 3 节点 Raft 集群
./kv_server --raft --cluster node-1:6379,node-2:6380,node-3:6381

# 客户端连任一节点
./kv_client -p 6379
> SET foo bar
OK

# 杀掉 Leader
# 验证：剩余节点选举出新 Leader，数据不丢失
```

### 一致性检验

- 任何节点查询同一 key 返回相同值
- 网络分区恢复后自动同步
- 少数节点故障不影响可用性

### 容错

- 5 节点集群最多容忍 2 节点故障
- Follower 重启后自动追日志
- Leader 旧了不能当选（选举限制）
