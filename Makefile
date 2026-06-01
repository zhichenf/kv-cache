.PHONY: build clean test server client

BUILD_DIR = build
GTEST_LIB_DIR = third_party/lib/win

# 构建所有目标，并将 Google Test 动态库拷贝到 test 目录
build:
	@mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && cmake -G "MinGW Makefiles" .. && cmake --build .
	@cp -f $(GTEST_LIB_DIR)/*.dll $(BUILD_DIR)/test/ 2>/dev/null || true

# 运行测试
test: build
	@cd $(BUILD_DIR) && ./test/kv_tests.exe

# 启动服务端
server: build
	@cd $(BUILD_DIR) && ./bin/kv_server.exe

# 启动客户端（需要参数：host port）
client: build
	@cd $(BUILD_DIR) && ./bin/kv_client.exe $(ARGS)

# 清理构建产物
clean:
	@rm -rf $(BUILD_DIR)
