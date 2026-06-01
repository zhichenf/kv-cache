#include <gtest/gtest.h>
#include "common/protocol.h"

// 解析 SET 命令
TEST(ProtocolTest, ParseSet) {
    std::string data = "*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nalice\r\n";
    auto result = ParseCommand(data);
    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.cmd.type, CommandType::SET);
    EXPECT_EQ(result.cmd.args.size(), 2u);
    EXPECT_EQ(result.cmd.args[0], "name");
    EXPECT_EQ(result.cmd.args[1], "alice");
}

// 解析 GET 命令
TEST(ProtocolTest, ParseGet) {
    std::string data = "*2\r\n$3\r\nGET\r\n$4\r\nname\r\n";
    auto result = ParseCommand(data);
    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.cmd.type, CommandType::GET);
    EXPECT_EQ(result.cmd.args.size(), 1u);
    EXPECT_EQ(result.cmd.args[0], "name");
}

// 解析 DEL 命令
TEST(ProtocolTest, ParseDel) {
    std::string data = "*2\r\n$3\r\nDEL\r\n$4\r\nname\r\n";
    auto result = ParseCommand(data);
    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.cmd.type, CommandType::DEL);
    EXPECT_EQ(result.cmd.args.size(), 1u);
    EXPECT_EQ(result.cmd.args[0], "name");
}

// 解析 EXISTS 命令
TEST(ProtocolTest, ParseExists) {
    std::string data = "*2\r\n$6\r\nEXISTS\r\n$4\r\nname\r\n";
    auto result = ParseCommand(data);
    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.cmd.type, CommandType::EXISTS);
    EXPECT_EQ(result.cmd.args.size(), 1u);
    EXPECT_EQ(result.cmd.args[0], "name");
}

// 解析 KEYS 命令
TEST(ProtocolTest, ParseKeys) {
    std::string data = "*1\r\n$4\r\nKEYS\r\n";
    auto result = ParseCommand(data);
    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.cmd.type, CommandType::KEYS);
}

// 空数据返回 EMPTY
TEST(ProtocolTest, ParseEmpty) {
    std::string data = "";
    auto result = ParseCommand(data);
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.error, ParseError::EMPTY);
}

// 未知命令返回 UNKNOWN_COMMAND
TEST(ProtocolTest, ParseUnknown) {
    std::string data = "*1\r\n$4\r\nPING\r\n";
    auto result = ParseCommand(data);
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.error, ParseError::UNKNOWN_COMMAND);
}

// 参数不足返回 WRONG_ARG_COUNT
TEST(ProtocolTest, ParseWrongArgs) {
    std::string data = "*1\r\n$3\r\nSET\r\n";
    auto result = ParseCommand(data);
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.error, ParseError::WRONG_ARG_COUNT);
}

// SerializeResponse 序列化 OK
TEST(ProtocolTest, SerializeOk) {
    Response resp{Response::Status::OK, ""};
    EXPECT_EQ(SerializeResponse(resp), "+OK\r\n");
}

// SerializeResponse 序列化 VALUE
TEST(ProtocolTest, SerializeValue) {
    Response resp{Response::Status::VALUE, "alice"};
    EXPECT_EQ(SerializeResponse(resp), "$5\r\nalice\r\n");
}

// SerializeResponse 序列化 NOT_FOUND
TEST(ProtocolTest, SerializeNotFound) {
    Response resp{Response::Status::NOT_FOUND, ""};
    EXPECT_EQ(SerializeResponse(resp), "$-1\r\n");
}

// SerializeResponse 序列化 ERROR
TEST(ProtocolTest, SerializeError) {
    Response resp{Response::Status::ERROR, "unknown command"};
    EXPECT_EQ(SerializeResponse(resp), "-ERR unknown command\r\n");
}

// SerializeResponse 序列化 COUNT
TEST(ProtocolTest, SerializeCount) {
    Response resp{Response::Status::COUNT, "5"};
    EXPECT_EQ(SerializeResponse(resp), ":5\r\n");
}

// SerializeCommand 序列化命令
TEST(ProtocolTest, SerializeCommand) {
    Command cmd{CommandType::SET, {"name", "alice"}};
    std::string expected = "*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nalice\r\n";
    EXPECT_EQ(SerializeCommand(cmd), expected);
}
