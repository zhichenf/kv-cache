# Phase 4：分片（Sharding）

## 学习目标

- 理解水平分片与数据分布策略
- 掌握一致性哈希算法
- 理解虚拟节点与负载均衡
- 掌握分布式路由（Proxy vs Smart Client）

---

## 核心概念

### 为什么需要分片？

单机容量和性能有上限。分片将数据分散到多台机器上，每台只存一部分数据：

```
无分片：              分片后：
┌───────────┐      ┌──────┐ ┌──────┐ ┌──────┐
│  Node A   │      │Node A│ │Node B│ │Node C│
│ 全部数据   │      │1/3   │ │1/3   │ │1/3   │
└───────────┘      └──────┘ └──────┘ └──────┘
```

### 分片策略对比

| 策略 | 优点 | 缺点 |
|------|------|------|
| 范围分片（range） | 实现简单，范围查询友好 | 数据倾斜，rehash 代价大 |
| 哈希分片（mod N） | 数据均匀 | 增减节点需全量 rehash |
| **一致性哈希** | 增减节点只影响相邻节点 | 实现稍复杂 |

### 一致性哈希原理

```
将 hash 空间组织成环 (0 ~ 2^32-1)：

         NodeA
     ╱         ╲
    │     ●     │
    │           │
   NodeC        │
    │    ●      │
    │         ● │
     ╲         ╱
      ─────────
         NodeB

key 的 hash → 顺时针找到第一个节点
Node 增减 → 只影响相邻节点的数据
```

### 虚拟节点

每个物理节点映射到多个虚拟节点（通常 100~200 个），解决数据倾斜：

```
物理节点：NodeA, NodeB, NodeC
虚拟节点：NodeA-0, NodeA-1, ..., NodeB-0, ..., NodeC-159
总共 480 个虚拟节点分布在环上
```

---

## 系统设计

### 路由方式

**Smart Client** 方案（本阶段使用）：

```
Client ──── 路由表 ────▶ NodeA (shard 0)
                         NodeB (shard 1)
                         NodeC (shard 2)
```

1. Client 维护一致性哈希路由表
2. 对 key 计算 hash → 找到目标节点
3. 直接连接目标节点发送请求

### 路由表

```json
{
  "version": 3,
  "nodes": [
    {"id": "node-1", "host": "10.0.0.1", "port": 6379},
    {"id": "node-2", "host": "10.0.0.2", "port": 6379},
    {"id": "node-3", "host": "10.0.0.3", "port": 6379}
  ],
  "vnodes": 160
}
```

### 请求流程

```
Client 发起 GET key:
1. hash = consistent_hash(key)  // MD5 or XXHash
2. target = find_nearest_node(hash, vnode_ring)
3. connect(target.host, target.port)
4. send("GET key")
5. return response
```

### 节点扩缩容

**扩容**：新增 NodeD → 从 NodeC（顺时针相邻）迁移部分数据到 NodeD

**缩容**：下线 NodeA → NodeA 的数据迁移到顺时针下一个节点

---

## 接口变更

### 路由服务（可选）

可新增一个简单的配置中心：

```bash
./kv_router --port 6379
# 注册节点、查询路由表

# API:
# /nodes          GET   → 路由表 JSON
# /nodes/register POST  → 注册新节点
```

### Shard 管理命令

```
SHARDS\n
→ NodeA: key_range=[f00a, a1b2] count=150
  NodeB: key_range=[a1b2, c3d4] count=200
```

### 新增模块

```
src/router/
├── consistent_hash.h    // 一致性哈希环
├── consistent_hash.cpp
├── router.h             // 路由查询
├── router.cpp
```

---

## 一致性哈希接口

```cpp
class ConsistentHash {
public:
    void AddNode(const std::string& node_id);
    void RemoveNode(const std::string& node_id);
    std::string GetNode(const std::string& key) const;

private:
    std::map<uint32_t, std::string> ring_;  // hash → node
    size_t virtual_nodes_ = 160;
};
```

---

## 验收标准

### 路由正确性

```bash
# 启动 3 个存储节点
./kv_server --node-id node-1 --port 6379
./kv_server --node-id node-2 --port 6380
./kv_server --node-id node-3 --port 6381

# 使用 smart client
./kv_client --route nodes.json
> SET foo bar     # 自动路由到 node-2
OK
> GET foo         # 自动路由到 node-2
VALUE bar
```

### 扩缩容

- 添加节点后，只迁移约 1/N 的数据
- 删除节点后，数据平均分布到其他节点
- 每个节点的数据量大致均衡（±10%）

### 一致性验证

- 同一个 key 始终路由到同一节点（节点数量不变时）
- 节点增减后，不需要迁移的 key 仍路由到原节点
