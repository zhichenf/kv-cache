#pragma once

#include <cstdint>
#include <string>
#include <cstddef>

// 计算 CRC32 校验值（通用版，适用于任意数据）
uint32_t CRC32(const void* data, size_t len);

// 计算 CRC32 校验值（字符串版本）
uint32_t CRC32(const std::string& str);

// 计算 CRC32 校验值（单字节）
uint32_t CRC32(uint8_t byte);

// ============================================================
// crc namespace：CRC32 序列化/反序列化辅助函数
// ============================================================
namespace crc {

// 将 CRC32 追加到 output 末尾（4 字节小端序）
// 计算 data 的 CRC32，然后追加到 output
void AppendChecksum(std::string& output, const std::string& data);

// 从 data 读取 CRC32（4 字节小端序）
// data 必须至少有 4 字节
uint32_t ReadChecksum(const char* data);

// 验证 data[0..data_len-1] 的 CRC32 是否与存储在 data[data_len..data_len+3] 的 CRC32 匹配
// data_len 是实际数据长度（不含 CRC），总长度应为 data_len + 4
bool VerifyChecksum(const char* data, size_t data_len);

} // namespace crc
