#include <gtest/gtest.h>
#include "common/protocol.h"

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

// SerializeResponse 序列化 ERR
TEST(ProtocolTest, SerializeError) {
    Response resp{Response::Status::ERR, "unknown command"};
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

// ============================================================
// RespReader 测试
// ============================================================

// 完整数据一次性解析 SET
TEST(RespReaderTest, CompleteSet) {
    RespReader reader;
    std::string data = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    reader.Feed(data.data(), data.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::SET);
    EXPECT_EQ(cmd.args.size(), 2u);
    EXPECT_EQ(cmd.args[0], "foo");
    EXPECT_EQ(cmd.args[1], "bar");
}

// 完整数据一次性解析 GET
TEST(RespReaderTest, CompleteGet) {
    RespReader reader;
    std::string data = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    reader.Feed(data.data(), data.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::GET);
    EXPECT_EQ(cmd.args.size(), 1u);
    EXPECT_EQ(cmd.args[0], "foo");
}

// 半包场景：先 Feed 一半，再 Feed 另一半
TEST(RespReaderTest, PartialData) {
    RespReader reader;
    std::string full = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    
    // 只 Feed 一半
    std::string half = full.substr(0, full.size() / 2);
    reader.Feed(half.data(), half.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::PARTIAL);
    
    // Feed 剩余部分
    std::string rest = full.substr(full.size() / 2);
    reader.Feed(rest.data(), rest.size());
    
    result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::SET);
}

// 粘包场景：两条命令粘在一起
TEST(RespReaderTest, StickyPackets) {
    RespReader reader;
    std::string cmd1 = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    std::string cmd2 = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    std::string stuck = cmd1 + cmd2;
    
    reader.Feed(stuck.data(), stuck.size());
    
    // 解析第一条
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::GET);
    EXPECT_EQ(cmd.args[0], "foo");
    
    // 解析第二条
    result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::SET);
    EXPECT_EQ(cmd.args[1], "bar");
}

// 协议错误：非 RESP 格式
TEST(RespReaderTest, ProtocolError) {
    RespReader reader;
    std::string bad = "NOT RESP DATA\r\n";
    reader.Feed(bad.data(), bad.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::PROTOCOL_ERROR);
}

// 空缓冲区返回 PARTIAL
TEST(RespReaderTest, EmptyBuffer) {
    RespReader reader;
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::PARTIAL);
}

// 多次小量 Feed（字节级拆分）
TEST(RespReaderTest, ByteByByteFeed) {
    RespReader reader;
    std::string data = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    
    Command cmd;
    for (char c : data) {
        reader.Feed(&c, 1);
    }
    
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::GET);
    EXPECT_EQ(cmd.args[0], "foo");
}

// 未知命令
TEST(RespReaderTest, UnknownCommand) {
    RespReader reader;
    std::string data = "*1\r\n$4\r\nPING\r\n";
    reader.Feed(data.data(), data.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.error, ParseError::UNKNOWN_COMMAND);
}

// 参数不足
TEST(RespReaderTest, WrongArgCount) {
    RespReader reader;
    std::string data = "*1\r\n$3\r\nSET\r\n";
    reader.Feed(data.data(), data.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.error, ParseError::WRONG_ARG_COUNT);
}

// 解析后缓冲区清空，可以继续解析新命令
TEST(RespReaderTest, ReuseAfterParse) {
    RespReader reader;
    std::string data = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    reader.Feed(data.data(), data.size());
    
    Command cmd;
    reader.TryParse(cmd);
    
    // 再次 Feed 新命令
    std::string data2 = "*2\r\n$3\r\nDEL\r\n$3\r\nbar\r\n";
    reader.Feed(data2.data(), data2.size());
    
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::DEL);
    EXPECT_EQ(cmd.args[0], "bar");
}

// DEL 和 EXISTS 命令
TEST(RespReaderTest, DelAndExists) {
    RespReader reader;
    std::string del = "*2\r\n$3\r\nDEL\r\n$3\r\nkey\r\n";
    reader.Feed(del.data(), del.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::DEL);
    EXPECT_EQ(cmd.args[0], "key");
    
    std::string exists = "*2\r\n$6\r\nEXISTS\r\n$3\r\nkey\r\n";
    reader.Feed(exists.data(), exists.size());
    
    result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::EXISTS);
}

// KEYS 命令（无参数）
TEST(RespReaderTest, Keys) {
    RespReader reader;
    std::string data = "*1\r\n$4\r\nKEYS\r\n";
    reader.Feed(data.data(), data.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::KEYS);
    EXPECT_TRUE(cmd.args.empty());
}

// 半包 + 粘包组合场景
TEST(RespReaderTest, PartialThenSticky) {
    RespReader reader;
    std::string cmd1 = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    std::string cmd2 = "*2\r\n$3\r\nDEL\r\n$3\r\nbar\r\n";
    
    // 先 Feed 第一条的一半 + 第二条全部
    std::string half1 = cmd1.substr(0, cmd1.size() / 2);
    reader.Feed(half1.data(), half1.size());
    
    Command cmd;
    auto result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::PARTIAL);
    
    // Feed 第一条剩余 + 第二条全部
    std::string rest1 = cmd1.substr(cmd1.size() / 2);
    std::string combined = rest1 + cmd2;
    reader.Feed(combined.data(), combined.size());
    
    // 解析第一条
    result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::GET);
    
    // 解析第二条
    result = reader.TryParse(cmd);
    EXPECT_EQ(result, RespReader::Result::OK);
    EXPECT_EQ(cmd.type, CommandType::DEL);
}
