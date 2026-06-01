#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <mutex>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

// 简单的同步日志器，支持级别过滤和时间戳
class Logger {
public:
    static Logger& Instance() {
        static Logger logger;
        return logger;
    }

    // 设置日志级别，低于该级别的日志不输出
    void SetLevel(LogLevel level) {
        level_ = level;
    }

    // 输出一条日志，ERROR 级别输出到 cerr，其余到 cout
    void Log(LogLevel level, const std::string& file, int line, const std::string& msg) {
        if (level < level_) return;

        std::string level_str;
        switch (level) {
            case LogLevel::DEBUG: level_str = "DEBUG"; break;
            case LogLevel::INFO:  level_str = "INFO";  break;
            case LogLevel::WARN:  level_str = "WARN";  break;
            case LogLevel::ERROR: level_str = "ERROR"; break;
        }

        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << "[" << std::put_time(&tm, "%H:%M:%S") << "]"
            << "[" << level_str << "]"
            << "[" << file << ":" << line << "] "
            << msg;

        std::lock_guard<std::mutex> lock(mutex_);
        if (level == LogLevel::ERROR) {
            std::cerr << oss.str() << std::endl;
        } else {
            std::cout << oss.str() << std::endl;
        }
    }

private:
    Logger() = default;
    LogLevel level_ = LogLevel::INFO;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg)  Logger::Instance().Log(LogLevel::DEBUG, __FILE__, __LINE__, msg)
#define LOG_INFO(msg)   Logger::Instance().Log(LogLevel::INFO,  __FILE__, __LINE__, msg)
#define LOG_WARN(msg)   Logger::Instance().Log(LogLevel::WARN,  __FILE__, __LINE__, msg)
#define LOG_ERROR(msg)  Logger::Instance().Log(LogLevel::ERROR, __FILE__, __LINE__, msg)
