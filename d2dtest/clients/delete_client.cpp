#include "client/client.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

// 删除客户端：DEL + SET 操作
// 用法: delete_client [host] [port]
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
    
    int del_ok = 0, del_fail = 0;
    int set_ok = 0, set_fail = 0;
    
    // 删除 key_80 ~ key_89，然后重新写入
    for (int i = 80; i < 90; ++i) {
        std::ostringstream key_ss;
        key_ss << "key_" << std::setw(2) << std::setfill('0') << i;
        std::string key = key_ss.str();
        
        // DEL
        if (client.Delete(key)) {
            ++del_ok;
        } else {
            ++del_fail;
        }
        
        // SET 重新写入
        std::ostringstream val_ss;
        val_ss << "del_" << i;
        
        if (client.Set(key, val_ss.str())) {
            ++set_ok;
        } else {
            ++set_fail;
        }
    }
    
    std::cout << "DELETE DONE: DEL " << del_ok << "/" << del_fail
              << " SET " << set_ok << "/" << set_fail << std::endl;
    return (del_fail > 0 || set_fail > 0) ? 1 : 0;
}
