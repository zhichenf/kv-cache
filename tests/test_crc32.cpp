#include <gtest/gtest.h>
#include "common/crc32.h"

// 测试空数据的 CRC32
TEST(CRC32Test, EmptyData) {
    uint32_t crc = CRC32(nullptr, 0);
    EXPECT_EQ(crc, 0x00000000);
}

// 测试单字节的 CRC32
TEST(CRC32Test, SingleByte) {
    uint8_t data = 0x00;
    uint32_t crc = CRC32(&data, 1);
    EXPECT_EQ(crc, 0xD202EF8D);
}

// 测试字符串 "123456789" 的 CRC32（标准测试向量）
TEST(CRC32Test, StandardTestVector) {
    std::string data = "123456789";
    uint32_t crc = CRC32(data);
    EXPECT_EQ(crc, 0xCBF43926);
}

// 测试字符串 "Hello" 的 CRC32
TEST(CRC32Test, HelloString) {
    std::string data = "Hello";
    uint32_t crc = CRC32(data);
    EXPECT_EQ(crc, 0xF7D18982);
}

// 测试相同数据的 CRC32 一致性
TEST(CRC32Test, Consistency) {
    std::string data = "test data for consistency";
    uint32_t crc1 = CRC32(data);
    uint32_t crc2 = CRC32(data);
    EXPECT_EQ(crc1, crc2);
}

// 测试不同数据的 CRC32 不同
TEST(CRC32Test, DifferentData) {
    std::string data1 = "hello";
    std::string data2 = "world";
    uint32_t crc1 = CRC32(data1);
    uint32_t crc2 = CRC32(data2);
    EXPECT_NE(crc1, crc2);
}

// 测试二进制数据的 CRC32
TEST(CRC32Test, BinaryData) {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD};
    uint32_t crc = CRC32(data, sizeof(data));
    EXPECT_NE(crc, 0);
}

// 测试单字节 CRC32 函数
TEST(CRC32Test, SingleByteFunction) {
    uint32_t crc1 = CRC32(uint8_t(0x41));  // 'A'
    uint32_t crc2 = CRC32("A", 1);
    EXPECT_EQ(crc1, crc2);
}

// ============================================================
// crc namespace 测试
// ============================================================

// 测试 AppendChecksum 和 ReadChecksum
TEST(CRC32Test, AppendAndReadChecksum) {
    std::string data = "hello world";
    std::string output;
    
    crc::AppendChecksum(output, data);
    
    // output 应该是 4 字节的 CRC32
    ASSERT_EQ(output.size(), 4u);
    
    // ReadChecksum 应该返回正确的 CRC32
    uint32_t read_crc = crc::ReadChecksum(output.data());
    uint32_t expected_crc = CRC32(data);
    EXPECT_EQ(read_crc, expected_crc);
}

// 测试 VerifyChecksum（正确数据）
TEST(CRC32Test, VerifyChecksumValid) {
    std::string data = "test data for verification";
    std::string buffer = data;
    
    // 追加 CRC32
    crc::AppendChecksum(buffer, data);
    
    // 验证应该成功
    EXPECT_TRUE(crc::VerifyChecksum(buffer.data(), data.size()));
}

// 测试 VerifyChecksum（损坏数据）
TEST(CRC32Test, VerifyChecksumCorrupted) {
    std::string data = "test data for verification";
    std::string buffer = data;
    
    // 追加 CRC32
    crc::AppendChecksum(buffer, data);
    
    // 破坏数据
    buffer[0] = 'X';
    
    // 验证应该失败
    EXPECT_FALSE(crc::VerifyChecksum(buffer.data(), data.size()));
}

// 测试 VerifyChecksum（损坏 CRC）
TEST(CRC32Test, VerifyChecksumCorruptedCRC) {
    std::string data = "test data for verification";
    std::string buffer = data;
    
    // 追加 CRC32
    crc::AppendChecksum(buffer, data);
    
    // 破坏 CRC
    buffer[data.size()] = 'X';
    
    // 验证应该失败
    EXPECT_FALSE(crc::VerifyChecksum(buffer.data(), data.size()));
}

// 测试空数据的 AppendChecksum
TEST(CRC32Test, AppendChecksumEmpty) {
    std::string data;
    std::string output;
    
    crc::AppendChecksum(output, data);
    
    ASSERT_EQ(output.size(), 4u);
    
    uint32_t read_crc = crc::ReadChecksum(output.data());
    uint32_t expected_crc = CRC32(data);
    EXPECT_EQ(read_crc, expected_crc);
}

// 测试二进制数据的 AppendChecksum 和 VerifyChecksum
TEST(CRC32Test, BinaryDataChecksum) {
    std::string data;
    data += '\x00';
    data += '\x01';
    data += '\x02';
    data += '\xFF';
    data += '\xFE';
    
    std::string buffer = data;
    crc::AppendChecksum(buffer, data);
    
    EXPECT_TRUE(crc::VerifyChecksum(buffer.data(), data.size()));
}

// 测试大文件的 AppendChecksum
TEST(CRC32Test, LargeDataChecksum) {
    std::string data(10000, 'x');  // 10KB
    std::string buffer = data;
    
    crc::AppendChecksum(buffer, data);
    
    EXPECT_TRUE(crc::VerifyChecksum(buffer.data(), data.size()));
    EXPECT_EQ(buffer.size(), data.size() + 4);
}
