#include "ota.hpp"
#include "firmware_header.hpp"
#include "../config/partition_table.hpp"
#include "../3rdparty/ed25519/ed25519.h"
#include "posix.hpp"
#include <cstring>

namespace aurora {

extern "C" void sys_print(const char* str);

namespace flash_internal {
    constexpr uint32_t FLASH_CTRL_BASE = 0x400FD000;
    volatile uint32_t* const FMA = reinterpret_cast<uint32_t*>(FLASH_CTRL_BASE + 0x000);
    volatile uint32_t* const FMD = reinterpret_cast<uint32_t*>(FLASH_CTRL_BASE + 0x004);
    volatile uint32_t* const FMC = reinterpret_cast<uint32_t*>(FLASH_CTRL_BASE + 0x008);
    constexpr uint32_t FLASH_KEY = 0x71D50000;

    void erase_page(uint32_t address) {
        *FMA = address & ~(1024 - 1);
        *FMC = FLASH_KEY | 0x2; 
        while (*FMC & 0x2) {} 
    }

    void write_word(uint32_t address, uint32_t data) {
        *FMA = address;
        *FMD = data;
        *FMC = FLASH_KEY | 0x1; 
        while (*FMC & 0x1) {} 
    }
}

OtaManager::OtaManager() {}

void OtaManager::erase_partition(uint32_t start_addr, uint32_t size) {
    for (uint32_t addr = start_addr; addr < start_addr + size; addr += 1024) {
        flash_internal::erase_page(addr);
    }
}

void OtaManager::write_flash_word(uint32_t address, uint32_t data) {
    flash_internal::write_word(address, data);
}

bool OtaManager::verify_signature(uint32_t offset, uint32_t image_size, const uint8_t* expected_signature) {
    const uint8_t ROOT_PUBLIC_KEY[32] = {
        0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 
        0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 
        0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 
        0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
    };
    
    // Message payload starts after the 128-byte header
    const uint8_t* message = reinterpret_cast<const uint8_t*>(offset + 128);
    return ed25519_verify(expected_signature, message, image_size, ROOT_PUBLIC_KEY) != 0;
}

bool OtaManager::unpack_from_vfs(const char* filepath) {
    int fd = open(filepath, 0);
    if (fd < 0) {
        sys_print("[OTA] Failed to open update package\r\n");
        return false;
    }

    PartitionTable* pt = reinterpret_cast<PartitionTable*>(0x00007000);
    uint32_t part_b_offset = pt->magic == PT_MAGIC ? pt->part_b.offset : PT_PART_B.offset;
    uint32_t part_size     = pt->magic == PT_MAGIC ? pt->part_b.size   : PT_PART_B.size;

    // Erase Staging Partition
    erase_partition(part_b_offset, part_size);

    // Read the Header First
    FirmwareHeader header;
    int bytes_read = read(fd, &header, sizeof(FirmwareHeader));
    if (bytes_read != sizeof(FirmwareHeader) || header.magic != FIRMWARE_MAGIC) {
        sys_print("[OTA] Invalid firmware header in package\r\n");
        close(fd);
        return false;
    }

    if (header.image_size + sizeof(FirmwareHeader) > part_size) {
        sys_print("[OTA] Image too large for partition\r\n");
        close(fd);
        return false;
    }

    // Write header (But change status to UPDATE_PENDING)
    header.status = FirmwareStatus::UPDATE_PENDING;
    uint32_t* header_words = reinterpret_cast<uint32_t*>(&header);
    for (size_t i = 0; i < sizeof(FirmwareHeader) / 4; ++i) {
        write_flash_word(part_b_offset + (i * 4), header_words[i]);
    }

    // Write body in chunks
    uint8_t buffer[256];
    uint32_t current_offset = part_b_offset + sizeof(FirmwareHeader);
    uint32_t remaining = header.image_size;

    while (remaining > 0) {
        int to_read = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        bytes_read = read(fd, buffer, to_read);
        if (bytes_read <= 0) break;

        uint32_t* word_buf = reinterpret_cast<uint32_t*>(buffer);
        uint32_t words = (bytes_read + 3) / 4;
        for (size_t i = 0; i < words; ++i) {
            write_flash_word(current_offset + (i * 4), word_buf[i]);
        }
        
        current_offset += bytes_read;
        remaining -= bytes_read;
    }

    close(fd);

    if (remaining > 0) {
        sys_print("[OTA] EOF reached before full image read\r\n");
        return false;
    }

    // Verify written payload
    if (!verify_signature(part_b_offset, header.image_size, header.signature)) {
        sys_print("[OTA] Signature verification failed\r\n");
        erase_partition(part_b_offset, part_size); // Clear invalid image
        return false;
    }

    sys_print("[OTA] Unpack successful, rebooting...\r\n");
    reboot();
    return true;
}

void OtaManager::reboot() {
    volatile uint32_t* AIRCR = reinterpret_cast<uint32_t*>(0xE000ED0C);
    *AIRCR = (0x05FA0000 | 0x04); // SYSRESETREQ
    while (true) {} 
}

} // namespace aurora
