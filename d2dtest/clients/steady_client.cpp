#include "client/client.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

// 稳定客户端：每 10ms 写一个 key，模拟真实用户
// 用法: steady_client [host] [port]
int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    
    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = static_cast<uint16_t>(std::stoi(argv[2]));
    
    KvClient client;
    if (!client.Connect(host, port)) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    int ok = 0, fail = 0;
    
    // 写入 key_50 ~ key_79
    for (int i = 50; i < 80; ++i) {
        std::ostringstream key_ss, val_ss;
        key_ss << "key_" << std::setw(2) << std::setfill('0') << i;
        val_ss << "steady_" << i;
        
        if (client.Set(key_ss.str(), val_ss.str())) {
            ++ok;
        } else {
            ++fail;
        }
        
        // 模拟用户间隔
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "STEADY DONE: " << ok << " ok, " << fail << " fail" << std::endl;
    return fail > 0 ? 1 : 0;
}
