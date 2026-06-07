#include "client/client.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

// 修改客户端：读取 key，修改后写回
// 用法: modify_client [host] [port]
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
    
    int get_ok = 0, get_fail = 0;
    int set_ok = 0, set_fail = 0;
    
    // 读取 key_90 ~ key_99，修改后写回
    for (int i = 90; i < 100; ++i) {
        std::ostringstream key_ss;
        key_ss << "key_" << std::setw(2) << std::setfill('0') << i;
        std::string key = key_ss.str();
        
        // GET 读取当前值
        auto current = client.Get(key);
        if (current) {
            ++get_ok;
            
            // 修改值
            std::string new_value = "modify_" + std::to_string(i);
            
            // SET 写回
            if (client.Set(key, new_value)) {
                ++set_ok;
            } else {
                ++set_fail;
            }
        } else {
            ++get_fail;
            
            // key 不存在，写入新值
            std::ostringstream val_ss;
            val_ss << "modify_" << i;
            if (client.Set(key, val_ss.str())) {
                ++set_ok;
            } else {
                ++set_fail;
            }
        }
    }
    
    std::cout << "MODIFY DONE: GET " << get_ok << "/" << get_fail
              << " SET " << set_ok << "/" << set_fail << std::endl;
    return (get_fail > 0 || set_fail > 0) ? 1 : 0;
}
