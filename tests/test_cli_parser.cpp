#include <gtest/gtest.h>
#include "client/cli_parser.h"

// 解析 SET key value
TEST(CliParserTest, ParseSet) {
    CliParser parser;
    auto cmd = parser.Parse("SET name alice");
    ASSERT_TRUE(cmd.has_value());
    EXPECT_EQ(cmd->type, CommandType::SET);
    EXPECT_EQ(cmd->args.size(), 2u);
    EXPECT_EQ(cmd->args[0], "name");
    EXPECT_EQ(cmd->args[1], "alice");
}

// 解析 GET key
TEST(CliParserTest, ParseGet) {
    CliParser parser;
    auto cmd = parser.Parse("GET name");
    ASSERT_TRUE(cmd.has_value());
    EXPECT_EQ(cmd->type, CommandType::GET);
    EXPECT_EQ(cmd->args.size(), 1u);
    EXPECT_EQ(cmd->args[0], "name");
}

// 解析 DEL key
TEST(CliParserTest, ParseDel) {
    CliParser parser;
    auto cmd = parser.Parse("DEL name");
    ASSERT_TRUE(cmd.has_value());
    EXPECT_EQ(cmd->type, CommandType::DEL);
    EXPECT_EQ(cmd->args.size(), 1u);
    EXPECT_EQ(cmd->args[0], "name");
}

// 解析 EXISTS key
TEST(CliParserTest, ParseExists) {
    CliParser parser;
    auto cmd = parser.Parse("EXISTS name");
    ASSERT_TRUE(cmd.has_value());
    EXPECT_EQ(cmd->type, CommandType::EXISTS);
    EXPECT_EQ(cmd->args.size(), 1u);
    EXPECT_EQ(cmd->args[0], "name");
}

// 解析 KEYS
TEST(CliParserTest, ParseKeys) {
    CliParser parser;
    auto cmd = parser.Parse("KEYS");
    ASSERT_TRUE(cmd.has_value());
    EXPECT_EQ(cmd->type, CommandType::KEYS);
}

// 空行返回 nullopt
TEST(CliParserTest, ParseEmpty) {
    CliParser parser;
    auto cmd = parser.Parse("");
    EXPECT_FALSE(cmd.has_value());
}

// 未知命令返回 UNKNOWN
TEST(CliParserTest, ParseUnknown) {
    CliParser parser;
    auto cmd = parser.Parse("PING");
    ASSERT_TRUE(cmd.has_value());
    EXPECT_EQ(cmd->type, CommandType::UNKNOWN);
    EXPECT_EQ(cmd->error, ParseError::UNKNOWN_COMMAND);
}

// 参数不足返回 WRONG_ARG_COUNT
TEST(CliParserTest, ParseWrongArgs) {
    CliParser parser;
    auto cmd = parser.Parse("SET");
    ASSERT_TRUE(cmd.has_value());
    EXPECT_EQ(cmd->type, CommandType::UNKNOWN);
    EXPECT_EQ(cmd->error, ParseError::WRONG_ARG_COUNT);
}
