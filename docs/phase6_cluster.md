# Phase 6：集群管理与运维

## 学习目标

- 理解分布式系统中的故障检测
- 掌握 Gossip 协议实现
- 实现自动化成员管理和 rebalance
- 学习可观测性（Metrics / Logging / Tracing）

---

## 核心概念

### 故障检测

**为什么需要？** TCP 连接断开 ≠ 节点故障，网络抖动可能导致误判。

**SWIM Protocol**（Scalable Weakly-consistent Infection-style）：

```
每个节点定期：
  1. 随机选一个节点发送 Ping
  2. 如果 Ping 超时 → 通过 k 个节点发送间接 Ping (PingReq)
  3. 间接 Ping 也超时 → 标记为 Suspect
  4. Suspect 超时 → 标记为 Dead 并传播
```

### Gossip 协议

信息像病毒一样传播：

```
t=0: NodeA 知道 "NodeD 挂了"
t=1: NodeA 告诉 NodeB, NodeC
t=2: NodeB 告诉 NodeE, NodeF
t=3: 所有节点都知道
```

收敛时间：O(log N)，N 为节点数

---

## 系统设计

### 集群元信息

```json
{
  "cluster_name": "kv-cluster-1",
  "nodes": [
    {"id": "node-1", "host": "10.0.0.1", "port": 6379,
     "status": "alive", "role": "leader", "term": 5,
     "shards": [0, 1, 2], "load": 0.45}
  ],
  "version": 42
}
```

### 成员变更流程

```
添加节点:
1. Admin 发送 JOIN 请求到任一节点
2. 该节点通过 Raft 共识将新节点加入集群
3. 新节点加入后，触发 rebalance
4. 部分 shard 开始迁移

下线节点:
1. Admin 发送 LEAVE 请求
2. 数据迁移到其他节点
3. 确认数据迁移完成 → 节点正式下线
```

### 自动 Rebalance

**触发条件**：
- 节点加入/离开
- 节点负载过高/过低（超过阈值）
- 定时触发（如每 10 分钟）

**策略**：
- 计算每个节点的目标 Shard 数
- 从负载高的节点迁移到负载低的节点
- 限制迁移并发数（如 2 个并发）

---

## 操作命令

```bash
# 集群管理 CLI
./kv_admin

> JOIN 10.0.0.5:6379
Node added, rebalancing...

> LEAVE node-5
Node node-5 leaving, migrating data...

> STATUS
┌────────┬───────────┬───────┬──────┬─────────┐
│ Node   │ Status    │ Role  │ 负载  │ Shards  │
├────────┼───────────┼───────┼──────┼─────────┤
│ node-1 │ alive     │ leader│ 0.45 │ [0,1,2] │
│ node-2 │ alive     │ follwr│ 0.52 │ [3,4,5] │
│ node-3 │ alive     │ follwr│ 0.48 │ [6,7,8] │
│ node-4 │ dead      │ -     │ -    │ -       │
└────────┴───────────┴───────┴──────┴─────────┘

> REBALANCE
Rebalance started...

> METRICS
ops/sec: 12,450
avg_latency: 1.2ms
p99_latency: 4.5ms
active_connections: 342
```

---

## 实现步骤

| 步骤 | 内容 | 说明 |
|------|------|------|
| 1 | Membership 协议 | JOIN/LEAVE/PING/PONG 消息 |
| 2 | 故障检测 (SWIM) | Ping/PingReq/Suspect/Dead |
| 3 | Gossip 传播 | 定期交换成员状态 |
| 4 | 数据迁移 | 后台迁移 shard 数据 |
| 5 | 管理 CLI | kv_admin 工具 |
| 6 | 监控 Metrics | 请求量、延迟、负载 |

---

## 验收标准

### 集群管理

```bash
# 初始 3 节点集群
./kv_server --cluster node-1,node-2,node-3

# 扩容到 5 节点
./kv_admin join node-4:6379
./kv_admin join node-5:6379
# 验证：数据自动 rebalance，服务不中断

# 缩容
./kv_admin leave node-5
# 验证：数据迁移到其他节点后 node-5 下线
```

### 故障自动恢复

- Kill 一个节点 → 其他节点检测到 → 触发 rebalance
- 节点恢复 → 自动加入集群 → 重新分配 shard

### 稳定性

- 持续运行 24 小时无内存泄漏
- 单节点故障恢复时间 < 30 秒
- Rebalance 期间读写服务正常
