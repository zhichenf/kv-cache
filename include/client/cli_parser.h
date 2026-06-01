#pragma once

#include "common/protocol.h"
#include <optional>

// 将用户输入的文本命令解析为 Command 结构体
class CliParser {
public:
    std::optional<Command> Parse(const std::string& line);
};
