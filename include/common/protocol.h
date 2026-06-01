#pragma once

#include <string>
#include <vector>
#include <cstdint>

// 语法类型
enum class CommandType : int {
    SET,
    GET,
    DEL,
    EXISTS,
    KEYS,
    UNKNOWN
};

// 命令出错的类型
enum class ParseError {
    NONE,
    EMPTY,
    UNKNOWN_COMMAND,
    WRONG_ARG_COUNT,
    PROTOCOL_ERROR,
};

// kv_cache 命令，
// 包含解析出来的命令，命令类型，以及命令错误类型
struct Command {
    CommandType type = CommandType::UNKNOWN;
    std::vector<std::string> args;
    ParseError error = ParseError::NONE;
};

// 对命令解析的结果
struct ParseResult {
    Command cmd;
    ParseError error = ParseError::NONE;
    bool Ok() const { return error == ParseError::NONE; }
};

// 服务器对客户端命令回应的resp协议类型
struct Response {
    enum class Status {
        OK,
        VALUE,
        NOT_FOUND,
        ERROR,
        COUNT
    };
    Status status;
    std::string message;
};

// 从完整 RESP 数组中解析出命令
ParseResult ParseCommand(const std::string& data);

// 将 Response 序列化为 RESP 格式
std::string SerializeResponse(const Response& resp);

// 将命令序列化为 RESP 格式（客户端使用）
std::string SerializeCommand(const Command& cmd);

// RESP 缓冲读取器 —— 处理 TCP 流式数据
class RespReader {
public:
    // 解析器协议层面的错误，语法层面的错误在Command中
    enum class Result {
        OK,              // 成功解析出一条命令
        PARTIAL,         // 数据不足，等待更多
        PROTOCOL_ERROR,  // RESP 格式错误，已跳过该行
    };

    // 语法错误通过 cmd.type == UNKNOWN 表示
    // cmd.error 说明具体类型

    void Feed(const char* data, size_t len);
    Result TryParse(Command& cmd);

private:
    std::string buf_;
};
