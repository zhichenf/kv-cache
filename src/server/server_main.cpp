#include "server/kv_server.h"
#include "common/logger.h"
#include <csignal>
#include <atomic>

std::atomic<bool> g_running{true};

static void SignalHandler(int) {
    static std::atomic<int> count{0};
    if (count.fetch_add(1) > 0) {
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
        return;
    }
    g_running = false;
}

int main(int argc, char* argv[]) {
    // 加载配置
    std::string config_path = "config.txt";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        }
    }
    
    ServerConfig config = ServerConfig::Load(config_path);
    config.OverrideFromArgs(argc, argv);
    
    // 设置信号处理
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    // 创建并启动服务器
    KvServer server(config);
    if (!server.Start()) {
        return 1;
    }
    
    // 等待退出信号
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    server.Stop();
    Logger::Shutdown();
    return 0;
}
