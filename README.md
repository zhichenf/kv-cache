# KV Cache

一个基于 C++20 的内存 KV 存储系统，采用 RESP 协议通信。

```

## 可用命令

| 命令 | 格式 | 说明 |
|------|------|------|
| `SET` | `SET key value` | 写入键值对，已存在的 key 会被覆盖 |
| `GET` | `GET key` | 读取 key 对应的值，不存在返回 nil |
| `DEL` | `DEL key` | 删除 key，返回 1（成功）或 0（不存在） |
| `EXISTS` | `EXISTS key` | 检查 key 是否存在 |
| `KEYS` | `KEYS` | 返回当前所有 key 的总数 |
| `exit` | `exit` | 退出客户端 |
| `quit` | `quit` | 退出客户端 |

## 使用示例

```bash
# 终端 1：启动服务端
./kv_server

# 终端 2：启动客户端
./kv_client 127.0.0.1

> SET name alice
OK
> GET name
alice
> EXISTS name
exists
> DEL name
1
> KEYS
total keys: 0
> exit
```
