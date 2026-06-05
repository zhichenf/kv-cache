#include "storage/kv_store.h"
#include "storage/persistent_kv_store.h"
#include "server/tcp_server.h"
#include "common/protocol.h"
#include "common/logger.h"

#include <iostream>
#include <csignal>
#include <unordered_map>
#include <thread>
#include <chrono>

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

static void PrintUsage() {
    std::cout << "Usage: kv_server [options]\n"
              << "Options:\n"
              << "  -p, --port <port>       Server port (default: 6379)\n"
              << "  -d, --data-dir <path>   Data directory for persistence (default: ./data)\n"
              << "  -h, --help              Show this help message\n";
}

// 服务端入口：解析参数 → 创建 PersistentKvStore + TcpServer → 启动 → 等待 Ctrl+C
int main(int argc, char* argv[]) {
    uint16_t port = 6379;
    std::string data_dir = "./data";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-d" || arg == "--data-dir") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            PrintUsage();
            return 1;
        }
    }

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    PersistentKvStore store(data_dir);
    TcpServer server(port);

    std::unordered_map<int, RespReader> readers;        // 每个连接都要又一个reader

    server.SetOnMessage([&](int fd, const std::string& data) -> std::string {
        auto& reader = readers[fd];
        reader.Feed(data.data(), data.size());

        std::string result;

        while (true) {
            Command cmd;
            auto res = reader.TryParse(cmd);
            if (res == RespReader::Result::PARTIAL) {
                break;
            }

            if (res == RespReader::Result::PROTOCOL_ERROR) {
                result += "-ERR protocol error\r\n";
                continue;
            }

            if (cmd.type == CommandType::UNKNOWN) {
                if (cmd.error == ParseError::UNKNOWN_COMMAND) {
                    result += "-ERR unknown command\r\n";
                } else {
                    result += "-ERR wrong number of arguments\r\n";
                }
                continue;
            }

            Response resp;
            switch (cmd.type) {
                case CommandType::SET:
                    store.Set(cmd.args[0], cmd.args[1]);
                    resp = {Response::Status::OK, ""};
                    break;

                case CommandType::GET: {
                    auto val = store.Get(cmd.args[0]);
                    if (val) {
                        resp = {Response::Status::VALUE, *val};
                    } else {
                        resp = {Response::Status::NOT_FOUND, ""};
                    }
                    break;
                }

                case CommandType::DEL:
                    resp = {Response::Status::COUNT,
                            store.Delete(cmd.args[0]) ? "1" : "0"};
                    break;

                case CommandType::EXISTS:
                    if (store.Exists(cmd.args[0])) {
                        resp = {Response::Status::OK, ""};
                    } else {
                        resp = {Response::Status::NOT_FOUND, ""};
                    }
                    break;

                case CommandType::KEYS:
                    resp = {Response::Status::COUNT,
                            std::to_string(store.AllKeys().size())};
                    break;

                default:
                    resp = {Response::Status::ERROR, "unknown command"};
                    break;
            }

            result += SerializeResponse(resp);
        }

        return result;
    });

    if (!server.Start()) {
        LOG_ERROR("Failed to start server on port " + std::to_string(port));
        return 1;
    }

    LOG_INFO("Server started on port " + std::to_string(port));

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO("Shutting down...");
    server.Stop();
    return 0;
}
