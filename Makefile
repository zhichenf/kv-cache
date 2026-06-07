.PHONY: build clean test test_all server client bench d2d_test

BUILD_DIR = build
GTEST_LIB_DIR = third_party/lib/win

# 构建所有目标（增量编译）
build:
	@mkdir -p $(BUILD_DIR) 2>/dev/null || true
	@if [ ! -f $(BUILD_DIR)/Makefile ]; then cd $(BUILD_DIR) && cmake -G "MinGW Makefiles" ..; fi
	@cd $(BUILD_DIR) && cmake --build .
	@cp -f $(GTEST_LIB_DIR)/*.dll $(BUILD_DIR)/test/ 2>/dev/null || true
	@cp -f config.txt $(BUILD_DIR)/ 2>/dev/null || true

# 运行所有测试
test_all: build
	@cd $(BUILD_DIR) && ./test/test_kv_store.exe
	@cd $(BUILD_DIR) && ./test/test_protocol.exe
	@cd $(BUILD_DIR) && ./test/test_cli_parser.exe
	@cd $(BUILD_DIR) && ./test/test_crc32.exe
	@cd $(BUILD_DIR) && ./test/test_wal.exe
	@cd $(BUILD_DIR) && ./test/test_snapshot.exe
	@cd $(BUILD_DIR) && ./test/test_persistent_kv_store.exe

# 运行 KvStore 测试
test_kv_store: build
	@cd $(BUILD_DIR) && ./test/test_kv_store.exe

# 运行 Protocol 测试
test_protocol: build
	@cd $(BUILD_DIR) && ./test/test_protocol.exe

# 运行 CliParser 测试
test_cli_parser: build
	@cd $(BUILD_DIR) && ./test/test_cli_parser.exe

# 运行 CRC32 测试
test_crc32: build
	@cd $(BUILD_DIR) && ./test/test_crc32.exe

# 运行 WAL 测试
test_wal: build
	@cd $(BUILD_DIR) && ./test/test_wal.exe

# 运行 Snapshot 测试
test_snapshot: build
	@cd $(BUILD_DIR) && ./test/test_snapshot.exe

# 运行 PersistentKvStore 测试
test_persistent_kv_store: build
	@cd $(BUILD_DIR) && ./test/test_persistent_kv_store.exe

# 启动服务端
server: build
	@cd $(BUILD_DIR) && ./bin/kv_server.exe

# 启动客户端（需要参数：host port）
client: build
	@cd $(BUILD_DIR) && ./bin/kv_client.exe $(ARGS)

# 清理构建产物
clean:
	@taskkill //F //IM kv_server.exe 2>/dev/null || true
	@rm -rf $(BUILD_DIR)

# 运行性能测试
bench: build
	@mkdir -p $(BUILD_DIR)/bench
	@cp -f $(GTEST_LIB_DIR)/libbenchmark*.dll $(BUILD_DIR)/bench/ 2>/dev/null || true
	@cd $(BUILD_DIR) && ./bench/bench_wal.exe

# 运行 D2D 集成测试
d2d_test: build
	@python d2dtest/run_test.py
