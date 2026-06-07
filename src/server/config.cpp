#include "server/config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// 去除首尾空白
static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 转小写
static std::string ToLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

ServerConfig ServerConfig::Load(const std::string& path) {
    ServerConfig config;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Config file not found: " << path << ", using defaults\n";
        return config;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        key = ToLower(key);
        
        if (key == "port") {
            config.port = static_cast<uint16_t>(std::stoi(value));
        } else if (key == "data_dir") {
            config.data_dir = value;
        } else if (key == "log_level") {
            config.log_level = ToLower(value);
        } else if (key == "log_dir") {
            config.log_dir = value;
        } else if (key == "log_max_files") {
            config.log_max_files = std::stoul(value);
        } else if (key == "log_max_size_mb") {
            config.log_max_size_mb = std::stoul(value);
        }
    }
    
    return config;
}

void ServerConfig::Save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to save config to: " << path << "\n";
        return;
    }
    
    file << "# kv_cache server config\n";
    file << "port=" << port << "\n";
    file << "data_dir=" << data_dir << "\n";
    file << "log_level=" << log_level << "\n";
    file << "log_dir=" << log_dir << "\n";
    file << "log_max_files=" << log_max_files << "\n";
    file << "log_max_size_mb=" << log_max_size_mb << "\n";
}

void ServerConfig::OverrideFromArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-d" || arg == "--data-dir") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if ((arg == "-l" || arg == "--log-level") && i + 1 < argc) {
            log_level = ToLower(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: kv_server [options]\n"
                      << "Options:\n"
                      << "  -c, --config <path>      Config file (default: config.txt)\n"
                      << "  -p, --port <port>        Server port\n"
                      << "  -d, --data-dir <path>    Data directory\n"
                      << "  -l, --log-level <level>  Log level (debug/info/warn/error)\n"
                      << "  -h, --help               Show this help\n";
            exit(0);
        }
    }
}
