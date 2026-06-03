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
