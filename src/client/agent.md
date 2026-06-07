# client — 客户端

## 职责

提供 RESP 协议封装的 TCP 客户端和 CLI 交互界面。

## 文件

| 文件 | 职责 |
|------|------|
| `client.h` / `client.cpp` | RESP 协议封装的 TCP 客户端（RAII） |
| `cli_parser.h` / `cli_parser.cpp` | 用户文本命令解析器 |
| `client_main.cpp` | CLI 入口（循环读输入 → 解析 → 执行 → 打印结果） |

## 关键设计

### KvClient

- **RAII**: 析构函数自动断开连接
- **跨平台**: WinSock2 / POSIX socket
- **接口**: `Connect()`, `Disconnect()`, `Set()`, `Get()`, `Delete()`, `Exists()`, `Keys()`
- **内部**: `SendCommand()` 发送 RESP 命令并接收完整响应
- **响应解析**: 按 RESP 类型判断完整消息（`+/-/:` 一行结束，`$` 按长度读取）

### CliParser

- **功能**: 将用户输入文本转换为 `Command` 结构体
- **解析**: `istringstream` 分词，SET 特殊处理（`getline` 读取含空格的 value）
- **返回**: `std::optional<Command>`（空行返回 nullopt）

### client_main.cpp

- **交互流程**: `> ` 提示 → getline → CliParser 解析 → KvClient 执行 → 打印结果
- **参数**: `-h host` `-p port`（默认 127.0.0.1:6379）
