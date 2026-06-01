# Phase 3：主从复制

## 学习目标

- 理解主从复制原理与 CAP 理论
- 掌握异步复制与同步复制
- 实现 Replication Log / Offset 追踪
- 感性认识一致性模型（最终一致性 vs 强一致性）

---

## 核心概念

### 为什么需要复制？

```
单节点问题：
┌───────┐
│  Node  │  ← 挂了所有数据丢失，服务不可用
└───────┘

主从复制：
    ┌───────┐  ← 写
    │ Master │
    └───┬───┘
    ┌───┴───┐┌───┴───┐  ← 读（分担读压力）
    │ Slave1 ││ Slave2 │
    └───────┘└───────┘
```

### CAP 理论

| 属性 | 含义 | 复制中的体现 |
|------|------|-------------|
| **C**onsistency | 所有节点数据一致 | 同步复制保证，但增加延迟 |
| **A**vailability | 服务持续可用 | 异步复制，主挂了从可以顶 |
| **P**artition Tolerance | 分区容忍 | 网络分区时，AP 还是 CP？ |

> 这个阶段我们实现 **AP 系统**（异步复制），后续 Phase 5 Raft 实现 **CP 系统**。

### 异步 vs 同步复制

```
同步复制：                   异步复制：
Client               Client
  │                    │
  ▼                    ▼
Master ──▶ Slave1     Master ──▶ Slave1
  │         │          │         │
  │  wait   │          │  return OK
  │ ◀───────│          │  ─────┐
  │         │          │       ▼ 稍后到达
  │ ───▶ Slave2       │       Slave2
  │  wait   │         │
  │ ◀───────│         │
  ▼                   ▼
return OK            return OK
```

---

## 系统设计

### 架构

```
┌─────────────────────┐
│     Master Node      │
│  ┌───────────────┐  │
│  │   KvStore     │  │
│  └───────┬───────┘  │
│          │          │
│  ┌───────▼───────┐  │
│  │ ReplManager   │  │
│  │ ┌───────────┐ │  │
│  │ │ ReplLog   │ │  │
│  │ └───────────┘ │  │
│  └───────┬───────┘  │
└──────────┼──────────┘
           │
     ┌─────┴──────┐
     │            │
┌────▼───┐  ┌────▼───┐
│ Slave1 │  │ Slave2 │
└────────┘  └────────┘
```

### 复制日志格式

每条写操作在 Master 上分配一个单调递增的序号：

```
ReplLog Entry:
┌──────────┬──────────┬───────────────┐
│ Seq (8B) │ Op (1B)  │ Data (变长)   │
│          │ 0=SET    │ 编码后的KV    │
│          │ 1=DEL    │               │
└──────────┴──────────┴───────────────┘
```

### 复制协议（内部 TCP）

Master 额外监听一个**内部端口**用于复制，或者复用同一个端口用命令区分。

**方案**：内部复制端口（如 master_port + 1000）

```
Slave → Master:   SYNC\n
                  (首次全量同步) → Master 发送当前所有数据

Slave → Master:   REPL_OFFSET <seq>\n
                  (增量同步，从 seq 开始拉取)

Master → Slave:   REPL_ENTRY <seq> <op> <key> <value>\n
Master → Slave:   REPL_OK\n  (无新数据)
```

### Slave 状态

Slave 对外暴露只读接口（GET、EXISTS、KEYS），拒绝写操作。

```
收到 SET → 返回 ERR read-only slave
```

---

## 接口变更

### 新增配置

```bash
# Master 启动
./kv_server --port 6379 --repl-port 6380 --role master

# Slave 启动
./kv_server --port 6381 --role slave --master-host 127.0.0.1 --master-port 6380
```

### 新增命令

```
REPL_INFO\n
  → ROLE master\n
  → ROLE slave master_host=127.0.0.1 master_port=6380 repl_offset=1024\n
```

---

## 实现步骤

| 序号 | 内容 | 说明 |
|------|------|------|
| 1 | 复制日志 Ordering | Master 每次写操作生成 seq |
| 2 | 复制 Server | Master 监听复制端口 |
| 3 | 同步 Client | Slave 连接 Master 发送 SYNC |
| 4 | 全量同步 | Master dump 全量数据给 Slave |
| 5 | 增量同步 | 基于 offset 持续同步 |
| 6 | 只读限制 | Slave 拒绝写请求 |
| 7 | 心跳检测 | Slave 定期 ping Master |

---

## 验收标准

### 基本复制

```bash
# 终端 1: Master
./kv_server --port 6379 --repl-port 6380 --role master

# 终端 2: Slave
./kv_server --port 6381 --role slave --master-host 127.0.0.1 --master-port 6380

# 终端 3: 向 Master 写数据
./kv_client -p 6379
> SET foo bar
OK

# 终端 4: 从 Slave 读
./kv_client -p 6381
> GET foo
VALUE bar
> SET foo2 baz
ERR read-only slave
```

### 容错

- Slave 断开重连后能重新全量/增量同步
- Master 重启后，Slave 自动重连
- 强制 Kill Slave 不影响 Master 的正常服务
