# server — 服务端

## 职责

提供 TCP 网络层、服务端入口、配置解析，以及整合所有组件的核心编排类。

## 文件

| 文件 | 职责 |
|------|------|
| `tcp_server.h` / `tcp_server.cpp` | TCP 网络层（纯收发字节流，不涉及协议解析） |
| `kv_server.h` / `kv_server.cpp` | 服务端核心编排类（整合 PersistentKvStore + TcpServer + Protocol） |
| `config.h` / `config.cpp` | 配置文件解析（key=value 格式） |
| `server_main.cpp` | 服务端入口（加载配置 → 启动 KvServer → 等待信号 → 优雅退出） |

## 关键设计

### TcpServer

- **回调模式**: `OnMessageCallback = std::function<string(int, const string&)>`
- **线程模型**: accept 线程 + 每连接一个处理线程（`detach`）
- **跨平台**: WinSock2（`WSAStartup`）/ POSIX socket
- **优雅关闭**: `Stop()` 关闭 server_fd 中断 accept → join accept_thread

### KvServer

- **组合**: 持有 PersistentKvStore + TcpServer + `unordered_map<int, RespReader>`
- **每连接状态**: 为每个 fd 维护独立的 `RespReader`（处理 TCP 粘包）
- **初始化顺序**: Logger → PersistentKvStore → TcpServer → RegisterMessageHandler
- **消息处理**: 循环解析直到 PARTIAL → switch(cmd.type) → store_ 操作 → SerializeResponse
- **运行时配置**: `SetLogLevel()` 动态调整日志级别

### Config

- **格式**: `key=value`，支持 `#` 注释
- **CLI 覆盖**: `-p/--port`, `-d/--data-dir`, `-l/--log-level`, `-h/--help`
- **优先级**: CLI 参数 > 配置文件 > 默认值

### server_main.cpp

- **信号处理**: `g_running` atomic + SignalHandler（Ctrl+C 优雅退出，双击强制退出）
- **启动流程**: 加载 config.txt → CLI 覆盖 → 创建 KvServer → Start() → 等待退出 → Stop() → Logger::Shutdown()
