#include "common/wal.h"
#include "common/crc32.h"
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

// ============================================================
// WALRecord 实现
// ============================================================

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
    
    // CRC32 (4 bytes, little-endian)
    uint32_t crc = CRC32(data);
    std::string result;
    result += static_cast<char>(crc & 0xFF);
    result += static_cast<char>((crc >> 8) & 0xFF);
    result += static_cast<char>((crc >> 16) & 0xFF);
    result += static_cast<char>((crc >> 24) & 0xFF);
    result += data;
    
    return result;
}

WALRecord WALRecord::Deserialize(const char* data, size_t len, size_t& consumed) {
    if (len < 9) {
        consumed = 0;
        return {};
    }
    
    uint32_t stored_crc = static_cast<uint8_t>(data[0])
                        | (static_cast<uint8_t>(data[1]) << 8)
                        | (static_cast<uint8_t>(data[2]) << 16)
                        | (static_cast<uint8_t>(data[3]) << 24);
    
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
    : is_open_(false),
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
    
    // 最后刷盘
    if (is_open_) {
        Sync();
        file_.close();
    }
}

WAL& WAL::GetInstance() {
    static WAL instance;
    return instance;
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
        file_.close();
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
        file_.close();
        is_open_ = false;
    }
    
    enabled_ = false;
}

bool WAL::TryOpen() {
    for (int i = 0; i < max_retries_; ++i) {
        file_.open(filepath_, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
        if (file_.is_open()) {
            is_open_ = true;
            return true;
        }
        
        file_.clear();
        
        file_.open(filepath_, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        if (file_.is_open()) {
            is_open_ = true;
            return true;
        }
        
        file_.clear();
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
    
    file_.seekg(0, std::ios::beg);
    
    std::string content((std::istreambuf_iterator<char>(file_)),
                         std::istreambuf_iterator<char>());
    
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
    
    file_.seekg(0, std::ios::end);
    
    return records;
}

void WAL::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!enabled_) {
        return;
    }
    
    file_.close();
    file_.open(filepath_, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to clear WAL file: " + filepath_);
    }
}

void WAL::Sync() {
    if (!enabled_) {
        return;
    }
    
    file_.flush();
    
#ifdef _WIN32
    int fd = _fileno(reinterpret_cast<FILE*>(file_.rdbuf()));
    _commit(fd);
#else
    int fd = fileno(reinterpret_cast<FILE*>(file_.rdbuf()));
    fsync(fd);
#endif
}

std::string WAL::GetFilepath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return filepath_;
}

size_t WAL::GetFileSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return 0;
    }
    
    WAL* self = const_cast<WAL*>(this);
    std::streampos current_pos = self->file_.tellg();
    self->file_.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(self->file_.tellg());
    self->file_.seekg(current_pos);
    
    return size;
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
        // 等待 1 秒
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.sync_interval_ms));
        
        // 定期 fsync
        if (is_open_) {
            std::lock_guard<std::mutex> lock(mutex_);
            Sync();
        }
    }
}

void WAL::WriteRecord(const WALRecord& record) {
    std::string data = record.Serialize();
    Write(data.data(), data.size());
}

void WAL::Write(const char* data, size_t len) {
    file_.write(data, len);
    if (!file_.good()) {
        throw std::runtime_error("Failed to write to WAL file");
    }
}

size_t WAL::Read(char* data, size_t len) {
    file_.read(data, len);
    return static_cast<size_t>(file_.gcount());
}
