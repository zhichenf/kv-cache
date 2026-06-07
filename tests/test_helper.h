#pragma once

#include "common/logger.h"
#include "server/config.h"
#include <string>
#include <algorithm>

// 解析日志级别字符串
inline LogLevel ParseLogLevelString(const std::string& level) {
    std::string lower = level;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "debug") return LogLevel::DEBUG;
    if (lower == "info")  return LogLevel::INFO;
    if (lower == "warn")  return LogLevel::WARN;
    if (lower == "error") return LogLevel::ERR;
    
    return LogLevel::INFO;
}

// 统一初始化 Logger，读取 config.txt
// 在 main() 中调用一次即可
inline void InitTestLogger() {
    std::string config_path = "config.txt";
    ServerConfig config = ServerConfig::Load(config_path);
    Logger::Init(config.log_dir, config.log_max_files,
                 config.log_max_size_mb * 1024 * 1024);
    Logger::Instance().SetLevel(ParseLogLevelString(config.log_level));
}
