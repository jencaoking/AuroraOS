// flash_device.hpp — Apollo3 内部 Flash 抽象层
// 为 LittleFS 提供 sector 级读/写/擦除接口。
// 真实硬件映射到 0x70000 起的 512KB App 分区。

#ifndef AURORA_VFS_FLASH_DEVICE_HPP
#define AURORA_VFS_FLASH_DEVICE_HPP

#include <stdint.h>
#include <stddef.h>
#include "board.h"

class FlashDevice {
public:
    static constexpr uint32_t SECTOR_SIZE = 4096;  // Apollo3 Flash 页大小

    FlashDevice() : base_addr_(APP_FLASH_BASE_ADDR), total_size_(APP_FLASH_SIZE) {}

    uint32_t get_size() const { return total_size_; }
    uint32_t get_sector_size() const { return SECTOR_SIZE; }

    // 读取 Flash 数据（直接内存映射，无需 I/O）
    int read(uint32_t offset, void* buf, uint32_t len) {
        if (offset + len > total_size_) return -1;
        const uint8_t* src = reinterpret_cast<const uint8_t*>(base_addr_ + offset);
        for (uint32_t i = 0; i < len; ++i) {
            static_cast<uint8_t*>(buf)[i] = src[i];
        }
        return static_cast<int>(len);
    }

    // 写入 Flash（需先擦除对应 sector）
    int write(uint32_t offset, const void* buf, uint32_t len) {
        if (offset + len > total_size_) return -1;
        // Apollo3 Flash 写入通过外设控制器（抽象简化）
        volatile uint32_t* const flash_ctrl = reinterpret_cast<volatile uint32_t*>(0x40018000);
        const uint8_t* src = static_cast<const uint8_t*>(buf);
        for (uint32_t i = 0; i < len; i += 4) {
            flash_ctrl[0] = base_addr_ + offset + i;
            uint32_t word = 0;
            for (uint32_t b = 0; b < 4 && (i + b) < len; ++b) {
                word |= static_cast<uint32_t>(src[i + b]) << (b * 8);
            }
            flash_ctrl[1] = word;
            flash_ctrl[2] = 0x01;  // 触发编程命令
        }
        return static_cast<int>(len);
    }

    // 擦除一个 sector（4KB）
    int erase_sector(uint32_t sector) {
        if (sector >= total_size_ / SECTOR_SIZE) return -1;
        volatile uint32_t* const flash_ctrl = reinterpret_cast<volatile uint32_t*>(0x40018000);
        flash_ctrl[0] = base_addr_ + sector * SECTOR_SIZE;
        flash_ctrl[2] = 0x02;  // 触发扇区擦除命令
        return 0;
    }

private:
    uint32_t base_addr_;
    uint32_t total_size_;
};

#endif // AURORA_VFS_FLASH_DEVICE_HPP
