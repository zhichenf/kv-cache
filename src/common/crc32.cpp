#include "common/crc32.h"

namespace {

// CRC32 标准多项式：0xEDB88320（反转形式）
constexpr uint32_t kCRC32Polynomial = 0xEDB88320;

// 预计算的 CRC32 查找表（256 项）
// 使用 constexpr 在编译期生成，避免手写出错
constexpr uint32_t MakeTable(uint32_t idx) {
    uint32_t crc = idx;
    for (int j = 0; j < 8; ++j) {
        crc = (crc & 1) ? ((crc >> 1) ^ kCRC32Polynomial) : (crc >> 1);
    }
    return crc;
}

struct CRC32Table {
    uint32_t data[256];
    constexpr CRC32Table() : data{} {
        for (uint32_t i = 0; i < 256; ++i) {
            data[i] = MakeTable(i);
        }
    }
};

constexpr CRC32Table kCRC32Table{};

// 计算单个字节的 CRC32
inline uint32_t CRC32Byte(uint32_t crc, uint8_t byte) {
    return kCRC32Table.data[(crc ^ byte) & 0xFF] ^ (crc >> 8);
}

} // namespace

// 计算 CRC32 校验值（通用版）
uint32_t CRC32(const void* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        crc = CRC32Byte(crc, bytes[i]);
    }
    return crc ^ 0xFFFFFFFF;
}

// 计算 CRC32 校验值（字符串版本）
uint32_t CRC32(const std::string& str) {
    return CRC32(str.data(), str.size());
}

// 计算 CRC32 校验值（单字节）
uint32_t CRC32(uint8_t byte) {
    return CRC32(&byte, 1);
}

// ============================================================
// crc namespace 实现
// ============================================================

namespace crc {

void AppendChecksum(std::string& output, const std::string& data) {
    uint32_t checksum = CRC32(data);
    output += static_cast<char>(checksum & 0xFF);
    output += static_cast<char>((checksum >> 8) & 0xFF);
    output += static_cast<char>((checksum >> 16) & 0xFF);
    output += static_cast<char>((checksum >> 24) & 0xFF);
}

uint32_t ReadChecksum(const char* data) {
    return static_cast<uint8_t>(data[0])
         | (static_cast<uint8_t>(data[1]) << 8)
         | (static_cast<uint8_t>(data[2]) << 16)
         | (static_cast<uint8_t>(data[3]) << 24);
}

bool VerifyChecksum(const char* data, size_t data_len) {
    uint32_t stored = ReadChecksum(data + data_len);
    uint32_t calculated = CRC32(data, data_len);
    return stored == calculated;
}

} // namespace crc
