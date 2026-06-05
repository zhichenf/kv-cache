#include "common/wal.h"
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

int FileOpenReadWrite(const char* path) {
#ifdef _WIN32
    return _open(path, _O_RDWR | _O_BINARY | _O_CREAT, _S_IREAD | _S_IWRITE);
#else
    return open(path, O_RDWR | O_CREAT, 0644);
#endif
}

int FileOpenTruncate(const char* path) {
#ifdef _WIN32
    return _open(path, _O_RDWR | _O_BINARY | _O_TRUNC | _O_CREAT, _S_IREAD | _S_IWRITE);
#else
    return open(path, O_RDWR | O_TRUNC | O_CREAT, 0644);
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

void FileSync(int fd) {
#ifdef _WIN32
    _commit(fd);
#else
    fsync(fd);
#endif
}

} // namespace

// ============================================================
// WALRecord 实现
// ============================================================

// 命令进行序列化并添加 CRC32 校验
std::string WALRecord::Serialize() const {
    std::string data;
    
    // OpType (1 byte)
    data += static_cast<char>(op);
    
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
    
    // CRC32 (4 bytes, little-endian) + data
    std::string result;
    crc::AppendChecksum(result, data);  // CRC 在前
    result += data;
    
    return result;
}

// 从二进制数据中反序列化 WALRecord，并验证 CRC32
WALRecord WALRecord::Deserialize(const char* data, size_t len, size_t& consumed) {
    if (len < 9) {
        consumed = 0;
        return {};
    }
    
    uint32_t stored_crc = crc::ReadChecksum(data);
    
    OpType op = static_cast<OpType>(data[4]);
    
    uint16_t key_len = static_cast<uint8_t>(data[5])
                     | (static_cast<uint8_t>(data[6]) << 8);
    
    size_t key_val_len_offset = 7;
    size_t min_required = 4 + 1 + 2 + key_len + 2;
    if (len < min_required) {
        consumed = 0;
        return {};
    }
    
    std::string key(data + key_val_len_offset, key_len);
    
    size_t val_len_offset = key_val_len_offset + key_len;
    uint16_t val_len = static_cast<uint8_t>(data[val_len_offset])
                     | (static_cast<uint8_t>(data[val_len_offset + 1]) << 8);
    
    size_t total_len = val_len_offset + 2 + val_len;
    if (len < total_len) {
        consumed = 0;
        return {};
    }
    
    std::string value(data + val_len_offset + 2, val_len);
    
    // 构建用于 CRC 验证的数据
    std::string data_for_crc;
    data_for_crc += static_cast<char>(op);
    data_for_crc += static_cast<char>(key_len & 0xFF);
    data_for_crc += static_cast<char>((key_len >> 8) & 0xFF);
    data_for_crc += key;
    data_for_crc += static_cast<char>(val_len & 0xFF);
    data_for_crc += static_cast<char>((val_len >> 8) & 0xFF);
    data_for_crc += value;
    
    uint32_t calculated_crc = CRC32(data_for_crc);
    
    if (stored_crc != calculated_crc) {
        consumed = 0;
        return {};
    }
    
    consumed = total_len;
    return {op, key, value};
}

// ============================================================
// WAL 实现（单例模式）
// ============================================================

WAL::WAL()
    : fd_(-1),
      write_pos_(0),
      is_open_(false),
      enabled_(false),
      max_retries_(3),
      running_(false) {
}

WAL::~WAL() {
    // 先停止线程（不持有锁，避免死锁）
    running_ = false;
    
    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }
    
    // 最后刷盘并关闭
    if (is_open_) {
        Sync();
        CloseFD();
    }
}

WAL& WAL::GetInstance() {
    static WAL instance;
    return instance;
}

void WAL::CloseFD() {
    if (fd_ >= 0) {
        FileClose(fd_);
        fd_ = -1;
    }
}

void WAL::Init(const std::string& filepath, const WALConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果已经初始化，先关闭
    if (is_open_) {
        running_ = false;
        if (sync_thread_.joinable()) {
            sync_thread_.join();
        }
        Sync();
        CloseFD();
        is_open_ = false;
    }
    
    config_ = config;
    filepath_ = filepath;
    enabled_ = TryOpen();
    
    // 启动后台刷盘线程（EVERYSEC 策略）
    if (enabled_ && config_.policy == FsyncPolicy::EVERYSEC) {
        running_ = true;
        sync_thread_ = std::thread(&WAL::SyncLoop, this);
    }
}

void WAL::Shutdown() {
    // 先停止线程（不持有锁，避免死锁）
    running_ = false;
    
    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }
    
    // 再加锁处理文件
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_open_) {
        Sync();
        CloseFD();
        is_open_ = false;
    }
    
    enabled_ = false;
}

bool WAL::TryOpen() {
    for (int i = 0; i < max_retries_; ++i) {
        // 先尝试以读写方式打开（追加模式）
        fd_ = FileOpenReadWrite(filepath_.c_str());
        if (fd_ >= 0) {
            // 移动到文件末尾（追加写入）
            write_pos_ = lseek(fd_, 0, SEEK_END);
            if (write_pos_ == static_cast<off_t>(-1)) {
                write_pos_ = 0;
            }
            is_open_ = true;
            return true;
        }
        
        // 清除错误状态，重试
#ifndef _WIN32
        // 检查是否是权限问题等不可恢复错误
        if (errno != ENOENT && errno != EACCES) {
            break;
        }
#endif
    }
    
    is_open_ = false;
    return false;
}

void WAL::Append(const WALRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!enabled_) {
        return;
    }
    
    // 直接写入文件
    WriteRecord(record);
    
    // ALWAYS 策略：立即 fsync
    if (config_.policy == FsyncPolicy::ALWAYS) {
        Sync();
    }
}

void WAL::AppendSet(const std::string& key, const std::string& value) {
    WALRecord record{OpType::SET, key, value};
    Append(record);
}

void WAL::AppendDel(const std::string& key) {
    WALRecord record{OpType::DEL, key, ""};
    Append(record);
}

std::vector<WALRecord> WAL::ReadAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<WALRecord> records;
    
    if (!is_open_) {
        return records;
    }
    
    // 获取文件大小
    off_t file_size = lseek(fd_, 0, SEEK_END);
    if (file_size <= 0) {
        return records;
    }
    
    // 先定位到文件开头
    lseek(fd_, 0, SEEK_SET);
    
    // 读取全部内容
    std::string content(file_size, '\0');
    char* ptr = content.data();
    ssize_t total_read = 0;
    while (total_read < file_size) {
        ssize_t n = FileRead(fd_, ptr, file_size - total_read);
        if (n <= 0) break;
        total_read += n;
        ptr += n;
    }
    
    content.resize(total_read);
    
    // 解析记录
    size_t offset = 0;
    while (offset < content.size()) {
        size_t consumed = 0;
        WALRecord record = WALRecord::Deserialize(
            content.data() + offset,
            content.size() - offset,
            consumed
        );
        
        if (consumed == 0) {
            break;
        }
        
        records.push_back(record);
        offset += consumed;
    }
    
    return records;
}

void WAL::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!enabled_) {
        return;
    }
    
    CloseFD();
    
    // 重新打开文件并清空
    fd_ = FileOpenTruncate(filepath_.c_str());
    
    if (fd_ < 0) {
        throw std::runtime_error("Failed to clear WAL file: " + filepath_);
    }
    
    write_pos_ = 0;
}

void WAL::Sync() {
    if (!enabled_ || fd_ < 0) {
        return;
    }
    FileSync(fd_);
}

std::string WAL::GetFilepath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return filepath_;
}

size_t WAL::GetFileSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_ || fd_ < 0) {
        return 0;
    }
    
    // 用 lseek 获取文件大小，不影响写入位置
    off_t cur = lseek(fd_, 0, SEEK_CUR);
    off_t end = lseek(fd_, 0, SEEK_END);
    lseek(fd_, cur, SEEK_SET);
    
    return static_cast<size_t>(end);
}

bool WAL::IsEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

FsyncPolicy WAL::GetPolicy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.policy;
}

void WAL::SyncLoop() {
    while (running_) {
        // 在锁内读取配置和检查状态，避免竞态条件
        int interval_ms = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            interval_ms = config_.sync_interval_ms;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        
        std::lock_guard<std::mutex> lock(mutex_);
        Sync();
    }
}

void WAL::WriteRecord(const WALRecord& record) {
    std::string data = record.Serialize();
    
    ssize_t n = FileWrite(fd_, data.data(), data.size());
    
    if (n != static_cast<ssize_t>(data.size())) {
        throw std::runtime_error("Failed to write to WAL file");
    }
    
    write_pos_ += n;
}
