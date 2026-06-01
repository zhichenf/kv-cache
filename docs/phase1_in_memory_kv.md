# Phase 1：单机内存版 KV Store

## 学习目标

- 掌握 C++ 多线程编程（读写锁）
- 理解 TCP socket 编程 (server + client)
- 学会设计简单的文本网络协议
- 掌握 CMake 构建与项目管理

---

## 核心概念

### 线程安全

多个线程同时访问同一数据结构会产生**数据竞争**。这里的 KV Store 需要支持：
- **读-读**：允许多个线程同时读
- **读-写**：禁止并发，写时不能读
- **写-写**：禁止并发

使用 `std::shared_mutex`：`shared_lock`（共享/读锁）和 `unique_lock`（独占/写锁）来实现。

### C10K 问题简述

当连接数增多时，每个连接一个线程的方式会浪费资源。这里我们先使用**一个 accept 线程 + 每个连接一个处理线程**的简单模型。后续可以改为 Reactor/Proactor 模型。

---

## 系统设计

### 架构图

```
┌──────────┐     TCP 文本协议     ┌───────────────────────┐
│ kv_client │ ──────────────────▶ │      kv_server         │
│ (CLI)     │ ◀────────────────── │                        │
└──────────┘                     │  ┌─────────────────┐   │
                                  │  │   KvStore        │   │
                                  │  │  (thread-safe)   │   │
                                  │  │  unordered_map   │   │
                                  │  └─────────────────┘   │
                                  │                        │
                                  │  ┌─────────────────┐   │
                                  │  │   TcpServer      │   │
                                  │  │  (multi-client)  │   │
                                  │  └─────────────────┘   │
                                  └───────────────────────┘
```

### 协议设计

**文本协议**，以 `\n`（换行符）作为消息分隔符。

请求格式：
```
CMD [arg1] [arg2] ...\n
```

| 命令 | 示例 | 说明 |
|------|------|------|
| SET | `SET name alice\n` | 设置 key=name, value=alice |
| GET | `GET name\n` | 获取 key=name 的值 |
| DEL | `DEL name\n` | 删除 key=name |
| EXISTS | `EXISTS name\n` | 检查 key 是否存在 |
| KEYS | `KEYS\n` | 返回所有 key 列表 |

响应格式：

| 响应 | 示例 | 说明 |
|------|------|------|
| `OK\n` | `OK` | 操作成功（无返回值） |
| `VALUE <val>\n` | `VALUE alice` | 返回单个值 |
| `NOT_FOUND\n` | `NOT_FOUND` | 键不存在 |
| `ERR <msg>\n` | `ERR unknown command` | 错误信息 |
| `COUNT <n>\n` | `COUNT 2` | 返回数量（DEL、KEYS 等） |

---

## 接口规范

### KvStore（src/storage/kv_store.h）

| 方法 | 说明 | 锁类型 |
|------|------|--------|
| `Set(key, value)` | 写入键值对 | unique_lock |
| `Get(key)` | 读取值，不存在返回 nullopt | shared_lock |
| `Delete(key)` | 删除键，返回值是否存在 | unique_lock |
| `Exists(key)` | 键是否存在 | shared_lock |
| `AllKeys()` | 返回所有 key | shared_lock |
| `Size()` | 当前键数量 | shared_lock |

### 自定义解析实现

`ParseCommand` 实现逻辑：
1. 去除尾部 `\r\n` 或 `\n`
2. 按空格分割字符串
3. 第一个 token 匹配命令类型
4. 根据命令类型检查参数个数：
   - `SET`: 需要 2 个参数（key, value）
   - `GET` / `DEL` / `EXISTS`: 需要 1 个参数
   - `KEYS`: 需要 0 个参数
5. 返回 `Command` 结构体，格式错误返回 `nullopt`

### TcpServer 工作流程

```
Start()
  ├─ 创建 socket (socket())
  ├─ 绑定地址 (bind())
  ├─ 监听 (listen())
  ├─ 启动 accept 线程
  │    └─ 循环 accept()
  │         └─ 每个连接启动 HandleClient 线程
  │              ├─ 循环 recv() 读取数据
  │              ├─ 调用 on_message_ 回调（由 main 传入）
  │              └─ send() 返回结果
  └─ 等待 Stop() 信号

Stop()
  ├─ 设置 running_ = false
  ├─ close(server_fd_) 中断 accept
  └─ join(accept_thread_)
```

### Windows 平台注意事项

- 需要调用 `WSAStartup()` / `WSACleanup()` — 放在 main 中
- 使用 `closesocket()` 而非 `close()`
- Socket 错误码用 `WSAGetLastError()`
- 设置 socket 为非阻塞（可选）

---

## 实现步骤（建议顺序）

| 序号 | 文件 | 工作量 | 说明 |
|------|------|--------|------|
| 1 | `src/storage/kv_store.cpp` | ★☆☆☆☆ | 纯数据结构，无网络依赖 |
| 2 | `src/server/protocol.cpp` | ★★☆☆☆ | 字符串解析 |
| 3 | `src/client/client.cpp` | ★★☆☆☆ | TCP 客户端（比服务端简单） |
| 4 | `src/server/tcp_server.cpp` | ★★★★☆ | 最复杂的部分，多线程 socket |
| 5 | `src/server_main.cpp` | ★☆☆☆☆ | 组装依赖，启动服务 |
| 6 | `src/client_main.cpp` | ★☆☆☆☆ | CLI 循环 |

---

## 验收标准

### 功能测试

```bash
# 终端 1：启动服务端
./kv_server 6379

# 终端 2：启动客户端
./kv_client 127.0.0.1 6379

# 客户端交互
> SET name alice
OK
> GET name
VALUE alice
> EXISTS name
OK
> DEL name
OK
> GET name
NOT_FOUND
> KEYS
COUNT 0
> INVALID
ERR unknown command
```

### 并发测试

启动多个客户端同时读写，不能出现 crash、死锁、数据不一致。

### 边界情况

- Value 中包含空格（完整 value 应被正确保存和返回）
- Value 为空字符串
- 超长 key/value（> 64KB）
- 同时大量连接（> 100 个）
- 客户端断开后服务端不 crash

---

## 提示与 FAQ

**Q: recv() 一次没收到完整的一行怎么办？**
A: 在 session 里用 `std::string` 做缓冲区，每次 recv 后将数据追加到缓冲区，然后不断尝试从缓冲区提取完整的 `\n` 结尾的消息。不完整的数据留在缓冲区等下一次 recv。

**Q: 服务端怎么优雅关闭？**
A: 设置 `std::atomic<bool> running_`，accept 循环检测这个标志。Stop() 时设置标志 + close socket，使 accept 返回错误从而退出循环。

**Q: Windows 和 Linux 差异怎么处理？**
A: 可以定义跨平台宏：
```cpp
#ifdef _WIN32
  #include <winsock2.h>
  #include <windows.h>
  typedef SOCKET socket_t;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  typedef int socket_t;
#endif
```
