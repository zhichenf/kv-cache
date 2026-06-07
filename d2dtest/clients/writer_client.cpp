#include "client/client.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>

// 预写入 100 个 key（key_00 ~ key_99），value = "value_XX"
// 用法: writer_client [host] [port]
int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    
    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = static_cast<uint16_t>(std::stoi(argv[2]));
    
    KvClient client;
    if (!client.Connect(host, port)) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        return 1;
    }
    
    int success = 0;
    int fail = 0;
    
    for (int i = 0; i < 100; ++i) {
        std::ostringstream key_ss, val_ss;
        key_ss << "key_" << std::setw(2) << std::setfill('0') << i;
        val_ss << "value_" << std::setw(2) << std::setfill('0') << i;
        
        std::string key = key_ss.str();
        std::string value = val_ss.str();
        
        if (client.Set(key, value)) {
            ++success;
        } else {
            ++fail;
            std::cerr << "FAIL SET " << key << std::endl;
        }
    }
    
    std::cout << "WRITER DONE: " << success << " ok, " << fail << " fail" << std::endl;
    return fail > 0 ? 1 : 0;
}
