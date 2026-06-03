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
