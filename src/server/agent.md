# server — 服务端

## 职责

提供 TCP 网络层和服务端入口，只负责建立连接和收发原始字节流，不涉及协议解析。

## 文件

| 文件 | 职责 |
|------|------|
| `tcp_server.h` | 导出 TcpServer 类声明 |
| `tcp_server.cpp` | 实现 socket/bind/listen/accept/recv/send 循环 |
| `server_main.cpp` | 服务端入口，组合 KvStore + TcpServer，处理 Ctrl+C 退出 |
