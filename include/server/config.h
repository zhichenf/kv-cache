#pragma once

#include <string>
#include <cstdint>

struct ServerConfig {
    uint16_t port = 6379;
    std::string data_dir = "./data";
    std::string log_level = "info";
    std::string log_dir = "logs";
    size_t log_max_files = 5;
    size_t log_max_size_mb = 5;

    // 从配置文件加载
    static ServerConfig Load(const std::string& path);
    
    // 保存到配置文件
    void Save(const std::string& path) const;
    
    // 命令行参数覆盖
    void OverrideFromArgs(int argc, char* argv[]);
};
