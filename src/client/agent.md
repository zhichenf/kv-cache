# client — 客户端

## 职责

提供 RESP 协议封装和 CLI 交互界面，连接服务端进行 KV 操作。

## 文件

| 文件 | 职责 |
|------|------|
| `client.h`   | 导出 KvClient 类声明 |
| `client.cpp` | 实现 TCP 连接和 Set/Get/Delete/Exists/Keys 操作 |
| `cli_parser.h` | 导出 CliParser 类声明 |
| `cli_parser.cpp` | 将用户文本输入解析为 Command 结构体 |
| `client_main.cpp` | CLI 入口，循环读用户输入→解析→调 KvClient→打印结果 |
