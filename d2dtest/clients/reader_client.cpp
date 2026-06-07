#include "client/client.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>

// 全局超时标志
static std::atomic<bool> g_timeout{false};

// 读取所有 key（key_00 ~ key_99），输出到文件
// 用法: reader_client [host] [port] [output_file] [timeout_sec]
int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    std::string output_file = "result.txt";
    int timeout_sec = 10;
    
    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = static_cast<uint16_t>(std::stoi(argv[2]));
    if (argc >= 4) output_file = argv[3];
    if (argc >= 5) timeout_sec = std::stoi(argv[4]);
    
    // 启动超时线程
    std::thread timeout_thread([timeout_sec]() {
        std::this_thread::sleep_for(std::chrono::seconds(timeout_sec));
        g_timeout = true;
    });
    timeout_thread.detach();
    
    KvClient client;
    if (!client.Connect(host, port)) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        return 1;
    }
    
    std::ofstream ofs(output_file);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open " << output_file << std::endl;
        return 1;
    }
    
    int found = 0;
    int missing = 0;
    
    for (int i = 0; i < 100; ++i) {
        if (g_timeout) {
            std::cerr << "TIMEOUT after reading " << (found + missing) << " keys" << std::endl;
            break;
        }
        
        std::ostringstream key_ss;
        key_ss << "key_" << std::setw(2) << std::setfill('0') << i;
        std::string key = key_ss.str();
        
        auto value = client.Get(key);
        if (value) {
            ofs << key << "=" << *value << std::endl;
            ++found;
        } else {
            ofs << key << "=MISSING" << std::endl;
            ++missing;
        }
    }
    
    std::cout << "READER DONE: " << found << " found, " << missing << " missing" << std::endl;
    return 0;
}
