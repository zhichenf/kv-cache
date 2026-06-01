#include "client/cli_parser.h"
#include <sstream>

namespace {

// 将字符串中的小写 ASCII 字母转为大写
std::string ToUpper(const std::string& s) {
    std::string r = s;
    for (char& c : r) {
        if (c >= 'a' && c <= 'z') {
            c -= 32;
        }
    }
    return r;
}

} // namespace

// 解析一行用户输入，返回 Command 结构体。空行返回 nullopt，语法错误通过 UNKNOWN 表示
std::optional<Command> CliParser::Parse(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;
    if (token.empty()) {
        return std::nullopt;
    }

    std::string upper = ToUpper(token);

    if (upper == "SET") {
        std::string key, value;
        iss >> key;
        if (key.empty()) {
            return Command{CommandType::UNKNOWN, {}, ParseError::WRONG_ARG_COUNT};
        }
        std::getline(iss >> std::ws, value);
        return Command{CommandType::SET, {key, value}, ParseError::NONE};
    }

    if (upper == "GET") {
        std::string key;
        iss >> key;
        if (key.empty()) {
            return Command{CommandType::UNKNOWN, {}, ParseError::WRONG_ARG_COUNT};
        }
        return Command{CommandType::GET, {key}, ParseError::NONE};
    }

    if (upper == "DEL") {
        std::string key;
        iss >> key;
        if (key.empty()) {
            return Command{CommandType::UNKNOWN, {}, ParseError::WRONG_ARG_COUNT};
        }
        return Command{CommandType::DEL, {key}, ParseError::NONE};
    }

    if (upper == "EXISTS") {
        std::string key;
        iss >> key;
        if (key.empty()) {
            return Command{CommandType::UNKNOWN, {}, ParseError::WRONG_ARG_COUNT};
        }
        return Command{CommandType::EXISTS, {key}, ParseError::NONE};
    }

    if (upper == "KEYS") {
        return Command{CommandType::KEYS, {}, ParseError::NONE};
    }

    return Command{CommandType::UNKNOWN, {}, ParseError::UNKNOWN_COMMAND};
}
