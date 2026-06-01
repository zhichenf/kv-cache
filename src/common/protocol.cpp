#include "common/protocol.h"
#include <sstream>
#include <cctype>
#include <optional>

namespace {

// 转为大写（ASCII 字符）
std::string ToUpper(const std::string& s) {
    std::string r = s;
    for (char& c : r) {
        if (c >= 'a' && c <= 'z') {
            c -= 32;
        }
    }
    return r;
}

// 从 pos 开始读取一行（RESP 要求 \r\n 结尾），返回行内容（不含 \r\n）
// 数据不足时返回 nullopt 且 pos 不变
std::optional<std::string> ReadLine(const std::string& buf, size_t& pos) {
    size_t crlf = buf.find("\r\n", pos);
    if (crlf == std::string::npos) {
        return std::nullopt;
    }
    std::string line = buf.substr(pos, crlf - pos);
    pos = crlf + 2;
    return line;
}

// 从 pos 开始解析 RESP 批量字符串 ($<len>\r\n<data>\r\n)
// 数据不足时返回 nullopt 且 pos 不变
std::optional<std::string> ReadBulkString(const std::string& buf, size_t& pos) {
    if (pos >= buf.size() || buf[pos] != '$') {
        return std::nullopt;
    }
    size_t saved = pos;

    auto line = ReadLine(buf, pos);
    if (!line) {
        pos = saved;
        return std::nullopt;
    }

    int len = std::stoi(line->substr(1));
    if (len == -1) {
        return std::nullopt;
    }

    if (pos + static_cast<size_t>(len) + 2 > buf.size()) {
        pos = saved;
        return std::nullopt;
    }

    std::string result = buf.substr(pos, len);
    pos += len + 2;
    return result;
}

} // namespace

// 从完整的 RESP 数组中解析出 Command
// 输入必须是 *<n>\r\n 数组格式，返回解析结果或错误码
ParseResult ParseCommand(const std::string& data) {
    size_t pos = 0;
    auto line = ReadLine(data, pos);
    if (!line) {
        return {{}, ParseError::EMPTY};
    }

    if (line->empty() || (*line)[0] != '*') {
        return {{}, ParseError::PROTOCOL_ERROR};
    }

    int count = std::stoi(line->substr(1));
    if (count <= 0) {
        return {{}, ParseError::EMPTY};
    }

    std::vector<std::string> tokens;
    for (int i = 0; i < count; ++i) {
        auto bulk = ReadBulkString(data, pos);
        if (!bulk) {
            return {{}, ParseError::PROTOCOL_ERROR};
        }
        tokens.push_back(std::move(*bulk));
    }

    if (tokens.empty()) {
        return {{}, ParseError::EMPTY};
    }

    std::string cmd_upper = ToUpper(tokens[0]);

    CommandType type;
    if (cmd_upper == "SET") {
        type = CommandType::SET;
    } else if (cmd_upper == "GET") {
        type = CommandType::GET;
    } else if (cmd_upper == "DEL") {
        type = CommandType::DEL;
    } else if (cmd_upper == "EXISTS") {
        type = CommandType::EXISTS;
    } else if (cmd_upper == "KEYS") {
        type = CommandType::KEYS;
    } else {
        return {{}, ParseError::UNKNOWN_COMMAND};
    }

    Command cmd{type, {}};

    switch (type) {
        case CommandType::SET: {
            if (tokens.size() < 3) {
                return {{}, ParseError::WRONG_ARG_COUNT};
            }
            cmd.args.push_back(tokens[1]);
            cmd.args.push_back(tokens[2]);
            break;
        }
        case CommandType::GET:
        case CommandType::DEL:
        case CommandType::EXISTS: {
            if (tokens.size() != 2) {
                return {{}, ParseError::WRONG_ARG_COUNT};
            }
            cmd.args.push_back(tokens[1]);
            break;
        }
        case CommandType::KEYS:
            break;
        default:
            return {{}, ParseError::UNKNOWN_COMMAND};
    }

    return {cmd, ParseError::NONE};
}

// 将 Response 转为 RESP 格式字符串
// OK → +OK\r\n | VALUE → $<len>\r\n<val>\r\n | NOT_FOUND → $-1\r\n
// ERROR → -ERR <msg>\r\n | COUNT → :<n>\r\n
std::string SerializeResponse(const Response& resp) {
    switch (resp.status) {
        case Response::Status::OK:
            return "+OK\r\n";
        case Response::Status::VALUE:
            return "$" + std::to_string(resp.message.size()) + "\r\n"
                   + resp.message + "\r\n";
        case Response::Status::NOT_FOUND:
            return "$-1\r\n";
        case Response::Status::ERROR:
            return "-ERR " + resp.message + "\r\n";
        case Response::Status::COUNT:
            return ":" + resp.message + "\r\n";
    }
    return "-ERR internal error\r\n";
}

// 将 Command 序列化为 RESP 数组格式（客户端发送命令用）
// SET name alice → *3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nalice\r\n
std::string SerializeCommand(const Command& cmd) {
    static const char* names[] = {"SET", "GET", "DEL", "EXISTS", "KEYS", "UNKNOWN"};
    std::string cmd_name = names[static_cast<int>(cmd.type)];
    int count = 1 + static_cast<int>(cmd.args.size());

    std::string resp = "*" + std::to_string(count) + "\r\n";
    resp += "$" + std::to_string(cmd_name.size()) + "\r\n" + cmd_name + "\r\n";
    for (const auto& arg : cmd.args) {
        resp += "$" + std::to_string(arg.size()) + "\r\n" + arg + "\r\n";
    }
    return resp;
}

// 向缓冲读取器追加 TCP 数据
void RespReader::Feed(const char* data, size_t len) {
    buf_.append(data, len);
}

// 从缓冲区尝试提取一个完整的 RESP 命令
// OK → 解析成功，cmd 有效或 cmd.type == UNKNOWN（见 cmd.error）
// PARTIAL → 数据不足，等待更多
// PROTOCOL_ERROR → RESP 格式错误，已跳过该行
RespReader::Result RespReader::TryParse(Command& cmd) {
    size_t pos = 0;
    size_t saved = pos;

    auto line = ReadLine(buf_, pos);
    if (!line) {
        return Result::PARTIAL;
    }

    if (line->empty() || (*line)[0] != '*') {
        buf_.erase(0, pos);
        return Result::PROTOCOL_ERROR;
    }

    int count = std::stoi(line->substr(1));
    if (count <= 0) {
        buf_.erase(0, pos);
        return Result::PROTOCOL_ERROR;
    }

    std::vector<std::string> tokens;
    for (int i = 0; i < count; ++i) {
        auto bulk = ReadBulkString(buf_, pos);
        if (!bulk) {
            pos = saved;
            return Result::PARTIAL;
        }
        tokens.push_back(std::move(*bulk));
    }

    std::string cmd_upper = ToUpper(tokens[0]);

    CommandType type;
    if (cmd_upper == "SET") {
        type = CommandType::SET;
    } else if (cmd_upper == "GET") {
        type = CommandType::GET;
    } else if (cmd_upper == "DEL") {
        type = CommandType::DEL;
    } else if (cmd_upper == "EXISTS") {
        type = CommandType::EXISTS;
    } else if (cmd_upper == "KEYS") {
        type = CommandType::KEYS;
    } else {
        buf_.erase(0, pos);
        cmd = {{}, {}, ParseError::UNKNOWN_COMMAND};
        return Result::OK;
    }

    switch (type) {
        case CommandType::SET: {
            if (tokens.size() < 3) {
                buf_.erase(0, pos);
                cmd = {{}, {}, ParseError::WRONG_ARG_COUNT};
                return Result::OK;
            }
            cmd = {type, {tokens[1], tokens[2]}};
            break;
        }
        case CommandType::GET:
        case CommandType::DEL:
        case CommandType::EXISTS: {
            cmd = {type, {tokens[1]}};
            break;
        }
        case CommandType::KEYS:
            cmd = {type, {}};
            break;
        default:
            cmd = {{}, {}, ParseError::UNKNOWN_COMMAND};
            buf_.erase(0, pos);
            return Result::OK;
    }

    buf_.erase(0, pos);
    return Result::OK;
}
