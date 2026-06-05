#include "common/snapshot.h"
#include "common/crc32.h"
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

// ============================================================
// 平台相关辅助函数
// ============================================================

namespace {

int FileOpenRead(const char* path) {
#ifdef _WIN32
    return _open(path, _O_RDONLY | _O_BINARY);
#else
    return open(path, O_RDONLY);
#endif
}

int FileOpenWrite(const char* path) {
#ifdef _WIN32
    return _open(path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
}

ssize_t FileRead(int fd, void* buf, size_t count) {
#ifdef _WIN32
    return _read(fd, buf, static_cast<unsigned int>(count));
#else
    return read(fd, buf, count);
#endif
}

ssize_t FileWrite(int fd, const void* buf, size_t count) {
#ifdef _WIN32
    return _write(fd, buf, static_cast<unsigned int>(count));
#else
    return write(fd, buf, count);
#endif
}

void FileClose(int fd) {
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
}

off_t FileSeek(int fd, off_t offset, int whence) {
#ifdef _WIN32
    return _lseek(fd, offset, whence);
#else
    return lseek(fd, offset, whence);
#endif
}

void FileDelete(const char* path) {
#ifdef _WIN32
    _unlink(path);
#else
    unlink(path);
#endif
}

} // namespace

// ============================================================
// SnapshotEntry 实现
// ============================================================

std::string SnapshotEntry::Serialize() const {
    std::string data;
    
    // KeyLen (2 bytes, little-endian)
    uint16_t key_len = static_cast<uint16_t>(key.size());
    data += static_cast<char>(key_len & 0xFF);
    data += static_cast<char>((key_len >> 8) & 0xFF);
    
    // Key
    data += key;
    
    // ValLen (2 bytes, little-endian)
    uint16_t val_len = static_cast<uint16_t>(value.size());
    data += static_cast<char>(val_len & 0xFF);
    data += static_cast<char>((val_len >> 8) & 0xFF);
    
    // Value
    data += value;
    
    // CRC32 (4 bytes, little-endian) 追加到末尾
    crc::AppendChecksum(data, data);
    
    return data;
}

SnapshotEntry SnapshotEntry::Deserialize(const char* data, size_t len, size_t& consumed) {
    if (len < 4) {  // 最小长度: 2(key_len) + 2(val_len) + 4(crc)
        consumed = 0;
        return {};
    }
    
    // KeyLen
    uint16_t key_len = static_cast<uint8_t>(data[0])
                     | (static_cast<uint8_t>(data[1]) << 8);
    
    size_t key_offset = 2;
    size_t min_required = 2 + key_len + 2 + 4;  // key_len + val_len + crc
    if (len < min_required) {
        consumed = 0;
        return {};
    }
    
    // Key
    std::string key(data + key_offset, key_len);
    
    size_t val_len_offset = key_offset + key_len;
    uint16_t val_len = static_cast<uint8_t>(data[val_len_offset])
                     | (static_cast<uint8_t>(data[val_len_offset + 1]) << 8);
    
    size_t val_offset = val_len_offset + 2;
    size_t total_len = val_offset + val_len + 4;  // +4 for CRC
    if (len < total_len) {
        consumed = 0;
        return {};
    }
    
    // Value
    std::string value(data + val_offset, val_len);
    
    // 验证 CRC（CRC 在末尾）
    size_t data_len = val_offset + val_len;  // 不含 CRC 的数据长度
    if (!crc::VerifyChecksum(data, data_len)) {
        consumed = 0;
        return {};
    }
    
    consumed = total_len;
    return {key, value};
}

// ============================================================
// Snapshot 实现（单例模式）
// ============================================================

Snapshot& Snapshot::GetInstance() {
    static Snapshot instance;
    return instance;
}

std::string Snapshot::GetSnapshotPath(const std::string& data_dir) const {
    return data_dir + "/snapshot.dat";
}

bool Snapshot::Create(const std::string& data_dir,
                      const std::vector<std::pair<std::string, std::string>>& entries) {
    std::string filepath = GetSnapshotPath(data_dir);
    return WriteSnapshot(filepath, entries);
}

bool Snapshot::Load(const std::string& data_dir,
                    std::vector<std::pair<std::string, std::string>>& entries) {
    std::string filepath = GetSnapshotPath(data_dir);
    return ReadSnapshot(filepath, entries);
}

bool Snapshot::IsValid(const std::string& data_dir) const {
    std::vector<std::pair<std::string, std::string>> entries;
    return const_cast<Snapshot*>(this)->Load(data_dir, entries);
}

bool Snapshot::Remove(const std::string& data_dir) {
    std::string filepath = GetSnapshotPath(data_dir);
    FileDelete(filepath.c_str());
    return true;
}

bool Snapshot::WriteSnapshot(const std::string& filepath,
                             const std::vector<std::pair<std::string, std::string>>& entries) {
    // 构建文件内容
    std::string content;
    
    // Magic (5 bytes)
    content.append(SnapshotHeader::MAGIC, 5);
    
    // Version (4 bytes, little-endian)
    uint32_t version = SnapshotHeader::VERSION;
    content += static_cast<char>(version & 0xFF);
    content += static_cast<char>((version >> 8) & 0xFF);
    content += static_cast<char>((version >> 16) & 0xFF);
    content += static_cast<char>((version >> 24) & 0xFF);
    
    // EntryCount (4 bytes, little-endian)
    uint32_t entry_count = static_cast<uint32_t>(entries.size());
    content += static_cast<char>(entry_count & 0xFF);
    content += static_cast<char>((entry_count >> 8) & 0xFF);
    content += static_cast<char>((entry_count >> 16) & 0xFF);
    content += static_cast<char>((entry_count >> 24) & 0xFF);
    
    // Entries
    for (const auto& entry : entries) {
        SnapshotEntry se{entry.first, entry.second};
        content += se.Serialize();
    }
    
    // FooterCRC (覆盖全部内容)
    crc::AppendChecksum(content, content);
    
    // 写入文件（原子操作：先写临时文件，再重命名）
    std::string tmp_path = filepath + ".tmp";
    
    int fd = FileOpenWrite(tmp_path.c_str());
    if (fd < 0) {
        return false;
    }
    
    // 写入全部内容
    const char* ptr = content.data();
    size_t remaining = content.size();
    while (remaining > 0) {
        ssize_t n = FileWrite(fd, ptr, remaining);
        if (n <= 0) {
            FileClose(fd);
            FileDelete(tmp_path.c_str());
            return false;
        }
        ptr += n;
        remaining -= n;
    }
    
    // fsync 并关闭
    FileClose(fd);
    
    // 重命名（原子操作）
    FileDelete(filepath.c_str());
    if (rename(tmp_path.c_str(), filepath.c_str()) != 0) {
        return false;
    }
    
    return true;
}

bool Snapshot::ReadSnapshot(const std::string& filepath,
                            std::vector<std::pair<std::string, std::string>>& entries) {
    int fd = FileOpenRead(filepath.c_str());
    if (fd < 0) {
        return false;
    }
    
    // 获取文件大小
    off_t file_size = FileSeek(fd, 0, SEEK_END);
    FileSeek(fd, 0, SEEK_SET);
    
    if (file_size <= 13) {  // 最小: 5(magic) + 4(version) + 4(count) + 4(footer_crc)
        FileClose(fd);
        return false;
    }
    
    // 读取全部内容
    std::string content(file_size, '\0');
    char* ptr = content.data();
    ssize_t total_read = 0;
    while (total_read < file_size) {
        ssize_t n = FileRead(fd, ptr, file_size - total_read);
        if (n <= 0) break;
        total_read += n;
        ptr += n;
    }
    
    FileClose(fd);
    
    if (total_read != file_size) {
        return false;
    }
    
    content.resize(total_read);
    
    // 验证 FooterCRC
    size_t data_len = content.size() - 4;  // 不含 CRC 的数据长度
    if (!crc::VerifyChecksum(content.data(), data_len)) {
        return false;
    }
    
    // 解析 Magic
    if (std::memcmp(content.data(), SnapshotHeader::MAGIC, 5) != 0) {
        return false;
    }
    
    // 解析 Version
    size_t offset = 5;
    uint32_t version = static_cast<uint8_t>(content[offset])
                     | (static_cast<uint8_t>(content[offset + 1]) << 8)
                     | (static_cast<uint8_t>(content[offset + 2]) << 16)
                     | (static_cast<uint8_t>(content[offset + 3]) << 24);
    offset += 4;
    
    if (version != SnapshotHeader::VERSION) {
        return false;
    }
    
    // 解析 EntryCount
    uint32_t entry_count = static_cast<uint8_t>(content[offset])
                         | (static_cast<uint8_t>(content[offset + 1]) << 8)
                         | (static_cast<uint8_t>(content[offset + 2]) << 16)
                         | (static_cast<uint8_t>(content[offset + 3]) << 24);
    offset += 4;
    
    // 解析 Entries
    entries.clear();
    entries.reserve(entry_count);
    
    for (uint32_t i = 0; i < entry_count; ++i) {
        size_t consumed = 0;
        SnapshotEntry entry = SnapshotEntry::Deserialize(
            content.data() + offset,
            content.size() - offset - 4,  // -4 for footer_crc
            consumed
        );
        
        if (consumed == 0) {
            return false;
        }
        
        entries.emplace_back(entry.key, entry.value);
        offset += consumed;
    }
    
    return true;
}
