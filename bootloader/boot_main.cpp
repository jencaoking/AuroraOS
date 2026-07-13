#include "kernel/firmware_header.hpp"
#include "config/partition_table.hpp"
#include "3rdparty/ed25519/ed25519.h"

extern "C" {
    extern uint32_t _sidata;
    extern uint32_t _sdata;
    extern uint32_t _edata;
    extern uint32_t _sbss;
    extern uint32_t _ebss;
}

namespace flash {

constexpr uint32_t FLASH_CTRL_BASE = 0x400FD000;
volatile uint32_t* const FMA = reinterpret_cast<uint32_t*>(FLASH_CTRL_BASE + 0x000);
volatile uint32_t* const FMD = reinterpret_cast<uint32_t*>(FLASH_CTRL_BASE + 0x004);
volatile uint32_t* const FMC = reinterpret_cast<uint32_t*>(FLASH_CTRL_BASE + 0x008);

constexpr uint32_t FLASH_KEY = 0x71D50000;

void erase_page(uint32_t address) {
    *FMA = address & ~(1024 - 1);
    *FMC = FLASH_KEY | 0x2; // Erase
    while (*FMC & 0x2) {} 
}

void write_word(uint32_t address, uint32_t data) {
    *FMA = address;
    *FMD = data;
    *FMC = FLASH_KEY | 0x1; // Write
    while (*FMC & 0x1) {} 
}

} // namespace flash

namespace {

const uint8_t ROOT_PUBLIC_KEY[32] = {
    // 32 bytes mock public key
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
};

bool verify_firmware(aurora::FirmwareHeader* header, uint32_t offset) {
    if (header->magic != aurora::FIRMWARE_MAGIC) return false;
    
    // Header size is 128 bytes
    const uint8_t* message = reinterpret_cast<const uint8_t*>(offset + 128);
    size_t message_len = header->image_size;
    
    return ed25519_verify(header->signature, message, message_len, ROOT_PUBLIC_KEY) != 0;
}

} // namespace

extern "C" void kernel_main() {
    using namespace aurora;

    PartitionTable* pt = reinterpret_cast<PartitionTable*>(0x00007000);

    uint32_t part_a_offset = pt->magic == PT_MAGIC ? pt->part_a.offset : PT_PART_A.offset;
    uint32_t part_b_offset = pt->magic == PT_MAGIC ? pt->part_b.offset : PT_PART_B.offset;
    uint32_t part_size     = pt->magic == PT_MAGIC ? pt->part_a.size   : PT_PART_A.size;

    FirmwareHeader* part_a = reinterpret_cast<FirmwareHeader*>(part_a_offset);
    FirmwareHeader* part_b = reinterpret_cast<FirmwareHeader*>(part_b_offset);

    bool a_valid = verify_firmware(part_a, part_a_offset);
    bool b_valid = verify_firmware(part_b, part_b_offset);

    // If B has a pending update and is valid, or if A is invalid and B is valid (recovery from interrupted swap)
    if (b_valid && (!a_valid || part_b->status == FirmwareStatus::UPDATE_PENDING)) {
        // Power-loss safe OTA Swap (B -> A)
        // 1. Erase A entirely
        for (uint32_t addr = part_a_offset; addr < part_a_offset + part_size; addr += 1024) {
            flash::erase_page(addr);
        }
        
        // 2. Copy BODY (skip first 128 bytes header)
        uint32_t words_to_copy = part_b->image_size / 4;
        if (part_b->image_size % 4 != 0) words_to_copy++;
        
        uint32_t* src = reinterpret_cast<uint32_t*>(part_b_offset + 128);
        uint32_t* dst = reinterpret_cast<uint32_t*>(part_a_offset + 128);
        for (uint32_t i = 0; i < words_to_copy; ++i) {
            flash::write_word(reinterpret_cast<uint32_t>(&dst[i]), src[i]);
        }
        
        // 3. Write HEADER last to ensure A is marked valid only when completely copied
        uint32_t* header_src = reinterpret_cast<uint32_t*>(part_b_offset);
        uint32_t* header_dst = reinterpret_cast<uint32_t*>(part_a_offset);
        for (uint32_t i = 0; i < 128 / 4; ++i) {
            // Modify status to VALID
            uint32_t word = header_src[i];
            if (i == 3) { // offset 12 is status
                word = static_cast<uint32_t>(FirmwareStatus::VALID);
            }
            flash::write_word(reinterpret_cast<uint32_t>(&header_dst[i]), word);
        }
        
        // Invalidate PART_B
        flash::erase_page(part_b_offset);
        
        // Re-check A
        a_valid = verify_firmware(part_a, part_a_offset);
    }

    if (a_valid) {
        // Boot PART_A
        uint32_t vector_table_addr = part_a_offset + 128; // The actual vector table starts after header
        
        volatile uint32_t* SCB_VTOR = reinterpret_cast<uint32_t*>(0xE000ED08);
        *SCB_VTOR = vector_table_addr;

        uint32_t app_sp = *reinterpret_cast<uint32_t*>(vector_table_addr);
        uint32_t app_pc = *reinterpret_cast<uint32_t*>(vector_table_addr + 4);

        asm volatile(
            "msr msp, %0\n"
            "bx %1\n"
            :: "r"(app_sp), "r"(app_pc)
        );
    }
    
    // Fallback: Loop forever
    while (true) {}
}
