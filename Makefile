.PHONY: build clean test test_all server client bench d2d_test

BUILD_DIR = build

# 检测平台
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Linux)
    GTEST_LIB_DIR = third_party/lib/linux
    EXE =
    KILL_CMD = killall
    CMAKE_GEN =
    PYTHON = python3
else
    GTEST_LIB_DIR = third_party/lib/win
    EXE = .exe
    KILL_CMD = taskkill //F //IM
    CMAKE_GEN = -G "MinGW Makefiles"
    PYTHON = python
endif

# 构建所有目标（增量编译）
build:
	@mkdir -p $(BUILD_DIR) 2>/dev/null || true
	@if [ ! -f $(BUILD_DIR)/Makefile ]; then \
		rm -rf $(BUILD_DIR)/CMakeCache.txt $(BUILD_DIR)/CMakeFiles 2>/dev/null || true; \
		cd $(BUILD_DIR) && cmake $(CMAKE_GEN) ..; \
	fi
	@$(MAKE) -C $(BUILD_DIR) --no-print-directory
	@if [ "$(UNAME_S)" != "Linux" ]; then cp -f $(GTEST_LIB_DIR)/*.dll $(BUILD_DIR)/test/ 2>/dev/null || true; fi
	@cp -f config.txt $(BUILD_DIR)/ 2>/dev/null || true

# 运行所有测试
test_all: build
	@cd $(BUILD_DIR) && ./test/test_kv_store$(EXE)
	@cd $(BUILD_DIR) && ./test/test_protocol$(EXE)
	@cd $(BUILD_DIR) && ./test/test_cli_parser$(EXE)
	@cd $(BUILD_DIR) && ./test/test_crc32$(EXE)
	@cd $(BUILD_DIR) && ./test/test_wal$(EXE)
	@cd $(BUILD_DIR) && ./test/test_snapshot$(EXE)
	@cd $(BUILD_DIR) && ./test/test_persistent_kv_store$(EXE)

# 运行 KvStore 测试
test_kv_store: build
	@cd $(BUILD_DIR) && ./test/test_kv_store$(EXE)

# 运行 Protocol 测试
test_protocol: build
	@cd $(BUILD_DIR) && ./test/test_protocol$(EXE)

# 运行 CliParser 测试
test_cli_parser: build
	@cd $(BUILD_DIR) && ./test/test_cli_parser$(EXE)

# 运行 CRC32 测试
test_crc32: build
	@cd $(BUILD_DIR) && ./test/test_crc32$(EXE)

# 运行 WAL 测试
test_wal: build
	@cd $(BUILD_DIR) && ./test/test_wal$(EXE)

# 运行 Snapshot 测试
test_snapshot: build
	@cd $(BUILD_DIR) && ./test/test_snapshot$(EXE)

# 运行 PersistentKvStore 测试
test_persistent_kv_store: build
	@cd $(BUILD_DIR) && ./test/test_persistent_kv_store$(EXE)

# 启动服务端
server: build
	@cd $(BUILD_DIR) && ./bin/kv_server$(EXE)

# 启动客户端（需要参数：host port）
client: build
	@cd $(BUILD_DIR) && ./bin/kv_client$(EXE) $(ARGS)

# 清理构建产物
clean:
	@$(KILL_CMD) kv_server$(EXE) 2>/dev/null || true
	@rm -rf $(BUILD_DIR)

# 运行性能测试
bench: build
	@mkdir -p $(BUILD_DIR)/bench
	@if [ "$(UNAME_S)" != "Linux" ]; then cp -f $(GTEST_LIB_DIR)/libbenchmark*.dll $(BUILD_DIR)/bench/ 2>/dev/null || true; fi
	@cd $(BUILD_DIR) && ./bench/bench_wal$(EXE)

# 运行 D2D 集成测试
d2d_test: build
	@$(PYTHON) d2dtest/run_test.py
