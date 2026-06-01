# common — 公共模块

## 职责

提供所有模块共享的基础设施：网络协议（RESP）、日志系统。

## 文件

| 文件 | 职责 |
|------|------|
| `protocol.h` | 定义 Command/Response/RespReader 等协议类型 |
| `protocol.cpp` | 实现 RESP 解析器、RespReader 缓冲读取、序列化 |
| `logger.h`   | 头文件仅日志器，支持级别过滤和时间戳 |
