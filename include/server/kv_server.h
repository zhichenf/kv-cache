#pragma once

#include "server/config.h"
#include "storage/persistent_kv_store.h"
#include "server/tcp_server.h"
#include "common/protocol.h"
#include "common/logger.h"
#include <string>
#include <memory>
#include <atomic>

class KvServer {
public:
    explicit KvServer(const ServerConfig& config);
    ~KvServer();
    
    // 禁止拷贝和移动
    KvServer(const KvServer&) = delete;
    KvServer& operator=(const KvServer&) = delete;
    
    // 启动服务
    bool Start();
    
    // 停止服务
    void Stop();
    
    // 获取配置
    const ServerConfig& GetConfig() const { return config_; }
    
    // 运行时更新日志级别
    void SetLogLevel(const std::string& level);
    
    // 获取存储实例（用于测试）
    PersistentKvStore& GetStore() { return *store_; }

private:
    // 注册消息处理回调
    void RegisterMessageHandler();
    
    // 解析日志级别字符串
    LogLevel ParseLogLevel(const std::string& level) const;
    
    ServerConfig config_;
    std::unique_ptr<PersistentKvStore> store_;
    std::unique_ptr<TcpServer> tcp_server_;
    std::unordered_map<int, RespReader> readers_;
    std::atomic<bool> running_{false};
};
