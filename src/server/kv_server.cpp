#include "server/kv_server.h"
#include "common/logger.h"
#include <iostream>

KvServer::KvServer(const ServerConfig& config) 
    : config_(config) {
    
    // 日志系统必须最先初始化，其他组件依赖它
    Logger::Init(config.log_dir, config.log_max_files, 
                 config.log_max_size_mb * 1024 * 1024);
    Logger::Instance().SetLevel(ParseLogLevel(config.log_level));
    
    // 再创建存储和网络组件
    store_ = std::make_unique<PersistentKvStore>(config.data_dir);
    tcp_server_ = std::make_unique<TcpServer>(config.port);
    
    // 注册消息处理回调
    RegisterMessageHandler();
}

KvServer::~KvServer() {
    Stop();
    Logger::Flush();
}

bool KvServer::Start() {
    if (running_) return true;
    
    if (!tcp_server_->Start()) {
        LOG_ERROR("Failed to start TCP server on port " + std::to_string(config_.port));
        return false;
    }
    
    running_ = true;
    LOG_INFO("KvServer started on port " + std::to_string(config_.port));
    LOG_INFO("Data directory: " + config_.data_dir);
    LOG_INFO("Log level: " + config_.log_level);
    
    return true;
}

void KvServer::Stop() {
    if (!running_) return;
    
    running_ = false;
    LOG_INFO("Shutting down KvServer...");
    tcp_server_->Stop();
    LOG_INFO("KvServer stopped");
}

void KvServer::SetLogLevel(const std::string& level) {
    config_.log_level = level;
    Logger::Instance().SetLevel(ParseLogLevel(level));
    LOG_INFO("Log level changed to: " + level);
}

LogLevel KvServer::ParseLogLevel(const std::string& level) const {
    std::string lower = level;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "debug") return LogLevel::DEBUG;
    if (lower == "info")  return LogLevel::INFO;
    if (lower == "warn")  return LogLevel::WARN;
    if (lower == "error") return LogLevel::ERR;
    
    return LogLevel::INFO;  // 默认
}

void KvServer::RegisterMessageHandler() {
    tcp_server_->SetOnMessage([this](int fd, const std::string& data) -> std::string {
        auto& shard = GetShard(fd);
        std::lock_guard lock(shard.mutex);
        
        auto& reader = shard.readers[fd];
        reader.Feed(data.data(), data.size());
        
        std::string result;
        
        while (true) {
            Command cmd;
            auto res = reader.TryParse(cmd);
            if (res == RespReader::Result::PARTIAL) {
                break;
            }
            
            if (res == RespReader::Result::PROTOCOL_ERROR) {
                result += "-ERR protocol error\r\n";
                continue;
            }
            
            if (cmd.type == CommandType::UNKNOWN) {
                if (cmd.error == ParseError::UNKNOWN_COMMAND) {
                    result += "-ERR unknown command\r\n";
                } else {
                    result += "-ERR wrong number of arguments\r\n";
                }
                continue;
            }
            
            Response resp;
            switch (cmd.type) {
                case CommandType::SET:
                    store_->Set(cmd.args[0], cmd.args[1]);
                    resp = {Response::Status::OK, ""};
                    break;
                    
                case CommandType::GET: {
                    auto val = store_->Get(cmd.args[0]);
                    if (val) {
                        resp = {Response::Status::VALUE, *val};
                    } else {
                        resp = {Response::Status::NOT_FOUND, ""};
                    }
                    break;
                }
                    
                case CommandType::DEL:
                    resp = {Response::Status::COUNT,
                            store_->Delete(cmd.args[0]) ? "1" : "0"};
                    break;
                    
                case CommandType::EXISTS:
                    if (store_->Exists(cmd.args[0])) {
                        resp = {Response::Status::OK, ""};
                    } else {
                        resp = {Response::Status::NOT_FOUND, ""};
                    }
                    break;
                    
                case CommandType::KEYS:
                    resp = {Response::Status::COUNT,
                            std::to_string(store_->AllKeys().size())};
                    break;
                    
                default:
                    resp = {Response::Status::ERR, "unknown command"};
                    break;
            }
            
            result += SerializeResponse(resp);
        }
        
        return result;
    });
}
