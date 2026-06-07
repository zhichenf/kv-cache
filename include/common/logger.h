#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <memory>
#include <string>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERR
};

class Logger {
public:
    static Logger& Instance() {
        static Logger logger;
        return logger;
    }

    static void Init(const std::string& log_dir = "logs",
                     size_t max_files = 5,
                     size_t max_size = 5 * 1024 * 1024) {
        auto& logger = Instance();
        
        // 创建异步线程池：队列深度 8192，1 个后台写入线程
        spdlog::init_thread_pool(8192, 1);
        
        std::string log_path = log_dir + "/kv_cache.log";
        
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path, max_size, max_files);
        
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        std::vector<spdlog::sink_ptr> sinks{file_sink, console_sink};
        
        // 创建异步 logger
        logger.spd_logger_ = std::make_shared<spdlog::async_logger>(
            "kv_cache", sinks.begin(), sinks.end(),
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        
        logger.spd_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%l][%s:%#] %v");
        logger.spd_logger_->set_level(spdlog::level::info);
        logger.spd_logger_->flush_on(spdlog::level::info);
        
        spdlog::set_default_logger(logger.spd_logger_);
    }

    static void Flush() {
        if (Instance().spd_logger_) {
            Instance().spd_logger_->flush();
        }
    }

    static void Shutdown() {
        Flush();
        spdlog::shutdown();
    }

    void SetLevel(LogLevel level) {
        if (!spd_logger_) return;
        
        switch (level) {
            case LogLevel::DEBUG: spd_logger_->set_level(spdlog::level::debug); break;
            case LogLevel::INFO:  spd_logger_->set_level(spdlog::level::info);  break;
            case LogLevel::WARN:  spd_logger_->set_level(spdlog::level::warn);  break;
            case LogLevel::ERR:   spd_logger_->set_level(spdlog::level::err);   break;
        }
    }

    std::shared_ptr<spdlog::logger> GetSpdLogger() { return spd_logger_; }

private:
    Logger() = default;
    std::shared_ptr<spdlog::logger> spd_logger_;
};

#define LOG_DEBUG(msg) SPDLOG_LOGGER_DEBUG(spdlog::default_logger(), msg)
#define LOG_INFO(msg)  SPDLOG_LOGGER_INFO(spdlog::default_logger(), msg)
#define LOG_WARN(msg)  SPDLOG_LOGGER_WARN(spdlog::default_logger(), msg)
#define LOG_ERROR(msg) SPDLOG_LOGGER_ERROR(spdlog::default_logger(), msg)
