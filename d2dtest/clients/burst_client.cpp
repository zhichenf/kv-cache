#include "client/client.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

// 突发客户端：无 sleep，瞬间写入 keys 0-49
// 用法: burst_client [host] [port]
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
    
    // 写入 key_00 ~ key_49
    for (int i = 0; i < 50; ++i) {
        std::ostringstream key_ss, val_ss;
        key_ss << "key_" << std::setw(2) << std::setfill('0') << i;
        val_ss << "burst_" << i;
        
        if (client.Set(key_ss.str(), val_ss.str())) {
            ++ok;
        } else {
            ++fail;
        }
    }
    
    std::cout << "BURST DONE: " << ok << " ok, " << fail << " fail" << std::endl;
    return fail > 0 ? 1 : 0;
}
