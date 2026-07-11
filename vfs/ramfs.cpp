#include "ramfs.hpp"

// 依赖全局重载的 operator new
RamFile::RamFile(int capacity) : capacity_(capacity), file_size_(0) {
    data_ = new char[capacity_];
}

RamFile::~RamFile() {
    delete[] data_;
}

int RamFile::read(char* buf, int len, int offset) {
    if (offset >= file_size_) return 0; // EOF (End of File)
    
    int bytes_to_read = len;
    if (offset + len > file_size_) {
        bytes_to_read = file_size_ - offset;
    }

    if (bytes_to_read > 0) {
        __builtin_memcpy(buf, data_ + offset, bytes_to_read);
    }
    return bytes_to_read;
}

int RamFile::write(const char* buf, int len, int offset) {
    if (offset >= capacity_) return 0; // 超出物理容量上限
    
    int bytes_to_write = len;
    if (offset + len > capacity_) {
        bytes_to_write = capacity_ - offset;
    }

    if (bytes_to_write > 0) {
        __builtin_memcpy(data_ + offset, buf, bytes_to_write);
    }

    // 动态更新文件逻辑大小
    if (offset + bytes_to_write > file_size_) {
        file_size_ = offset + bytes_to_write;
    }

    return bytes_to_write;
}
