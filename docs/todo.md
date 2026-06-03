# TODO

## Phase 1 完成项

- [✔] `kv_store` — 线程安全的内存 KV 存储
- [✔] `protocol` — RESP 协议解析与序列化
- [✔] `client` — TCP 客户端封装
- [✔] `tcp_server` — 阻塞 + 每连接一线程模型
- [✔] `server_main` — 服务端入口
- [✔] `client_main` — 客户端 CLI 入口

## Phase 2：持久化与崩溃恢复（阶段 1：简单同步 WAL）

- [✔] CRC32 校验算法（`crc32.h/cpp`）
- [ ] WAL 日志实现（`wal.h/cpp`）
- [ ] 持久化存储类（`persistent_kv_store.h/cpp`）
- [ ] 修改 `server_main` 支持 `--data-dir` 参数
- [ ] 更新 `CMakeLists.txt` 添加新源文件
- [ ] CRC32 单元测试（`tests/test_crc32.cpp`）
- [ ] WAL 单元测试（`tests/test_wal.cpp`）
- [ ] 构建并运行测试
- [ ] 手动崩溃恢复测试

## 待重构

- [ ] `tcp_server` 改用 Reactor 模型（epoll / kqueue / IOCP），替换当前阻塞 + 每连接一线程模式
- [ ] 单机内存结构优化（分片锁 / 跳表 / 无锁结构等）
- [ ] 热插拔内存管理（存储引擎接口抽象，支持运行时切换）

## 待优化

- [ ] 异步日志：内存队列 + 后台线程批量 flush
- [ ] 文件输出 + 轮转：按大小/时间切分日志文件
- [ ] 延迟格式化：日志级别不够时不执行字符串拼接
- [ ] 分级输出：DEBUG → 文件，ERROR → 终端+文件
