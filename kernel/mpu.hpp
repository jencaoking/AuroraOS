#ifndef AURORA_MPU_HPP
#define AURORA_MPU_HPP

#include <stdint.h>
#include "../utils/hmac_sha256.hpp"  // Crc32 namespace

// ─────────────────────────────────────────────────────────────────────────────
// KERNEL_ASSERT: Controlled halt on fatal invariant violation.
// In production this triggers IWDG starvation (watchdog-forced reset).
// In host tests the extern "C" weak stub just records the failure.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef AURORA_HOST_TEST
#include <cstdio>
#define KERNEL_ASSERT(cond, msg) \
    do { if (!(cond)) { \
        ::fprintf(stderr, "[KERNEL_ASSERT] %s  (file %s line %d)\n", \
                  (msg), __FILE__, __LINE__); \
    }} while(0)
#else
extern "C" void uart_puts(const char* s);
#define KERNEL_ASSERT(cond, msg) \
    do { if (!(cond)) { \
        uart_puts("[KERNEL_ASSERT] " msg "\n"); \
        /* Stall — IWDG will force a reset */ \
        while(1) {} \
    }} while(0)
#endif

// ─────────────────────────────────────────────────────────────────────────────
// SandboxDescriptor — descriptor for a per-task MPU sandbox region.
// The crc32 field covers all other fields to detect in-memory tampering.
// ─────────────────────────────────────────────────────────────────────────────
struct SandboxDescriptor {
    uintptr_t stack_base;    // Must be aligned to (1 << size_pow2)
    uint8_t   size_pow2;     // Region size = 2^size_pow2 bytes  [5..17]
    uint32_t  version;       // Monotonically increasing per-task version
    uint32_t  crc32;         // CRC32(stack_base | size_pow2 | version)

    // Compute and store the CRC32 over the other three fields.
    // Call this whenever stack_base / size_pow2 / version are updated.
    void seal() noexcept {
        const uint32_t words[3] = {
            static_cast<uint32_t>(stack_base),
            static_cast<uint32_t>(size_pow2),
            version
        };
        crc32 = Crc32::compute(
            reinterpret_cast<const uint8_t*>(words), sizeof(words));
    }

    [[nodiscard]] bool is_valid() const noexcept {
        const uint32_t words[3] = {
            static_cast<uint32_t>(stack_base),
            static_cast<uint32_t>(size_pow2),
            version
        };
        const uint32_t expected = Crc32::compute(
            reinterpret_cast<const uint8_t*>(words), sizeof(words));
        return crc32 == expected;
    }
};

class MPU {
private:
    // Cortex-M4 MPU 核心控制寄存器物理地址映射
    static constexpr uintptr_t MPU_TYPE = 0xE000ED90;
    static constexpr uintptr_t MPU_CTRL = 0xE000ED94;
    static constexpr uintptr_t MPU_RNR  = 0xE000ED98; // 区域选择寄存器
    static constexpr uintptr_t MPU_RBAR = 0xE000ED9C; // 区域基地址寄存器
    static constexpr uintptr_t MPU_RASR = 0xE000EDA0; // 区域属性和大小寄存器

    inline static volatile uint32_t* const reg_ctrl = reinterpret_cast<volatile uint32_t*>(MPU_CTRL);
    inline static volatile uint32_t* const reg_rnr  = reinterpret_cast<volatile uint32_t*>(MPU_RNR);
    inline static volatile uint32_t* const reg_rbar = reinterpret_cast<volatile uint32_t*>(MPU_RBAR);
    inline static volatile uint32_t* const reg_rasr = reinterpret_cast<volatile uint32_t*>(MPU_RASR);

public:
    // MPU 权限配置宏
    static constexpr uint32_t AP_NO_ACCESS  = 0b000;
    static constexpr uint32_t AP_PRIV_RW    = 0b001; // 仅内核特权态可读写
    static constexpr uint32_t AP_ALL_RW     = 0b011; // 内核与用户态均可读写
    static constexpr uint32_t AP_PRIV_RO    = 0b101; // 仅内核特权态只读
    static constexpr uint32_t AP_ALL_RO     = 0b110; // 内核与用户态均只读

    static MPU& instance() {
        static MPU mpu;
        return mpu;
    }

    // 禁用 MPU 进行重新配置
    void disable() {
        __asm__ volatile ("dmb\n\t" : : : "memory");
        *reg_ctrl = 0;
        __asm__ volatile ("dsb\n\t" "isb\n\t" : : : "memory");
    }

    // 启用 MPU，并开启硬件默认背景区（保证在没有配到的区域里，特权态依然可以正常操作硬件）
    void enable() {
        // Bit 0: 开启 MPU | Bit 2: PRIVDEFENA (特权背景区启用)
        *reg_ctrl = (1 << 2) | (1 << 0);
        
        // 启用 MemManage 异常 (SHCSR 寄存器的 MEMFAULTENA 位 - Bit 16)
        volatile uint32_t* shcsr = reinterpret_cast<volatile uint32_t*>(0xE000ED24);
        *shcsr |= (1 << 16);

        __asm__ volatile ("dsb\n\t" "isb\n\t" : : : "memory");
    }

    // 配置单个保护区域
    // region_num: 0~7 | base_addr: 需与 size 对齐 | size_power_of_2: 例如 256字节为 8 (2^8=256)
    void configure_region(uint8_t region_num, uintptr_t base_addr, uint8_t size_power_of_2, uint32_t ap, bool execute_never = false, bool is_device = false) {
        // Ensure base_addr is aligned to size
        uint32_t alignment_mask = (1 << size_power_of_2) - 1;
        if (base_addr & alignment_mask) {
            // Not aligned, behavior undefined or error. We'll force align for safety here or assert
            base_addr &= ~alignment_mask;
        }
        
        *reg_rnr = region_num;
        *reg_rbar = base_addr & ~0x1F; // 清空低5位配置位，只留下基地址

        uint32_t rasr_val = (1 << 0); // Bit 0: ENABLE
        rasr_val |= ((size_power_of_2 - 1) & 0x1F) << 1; // 设定内存区域大小
        rasr_val |= (ap & 0x7) << 24; // 设定访问权限
        
        if (is_device) {
            // Device memory attributes (B=1, C=0, TEX=0)
            rasr_val |= (1 << 16); 
        } else {
            // Normal memory attributes (Write-Back, Read/Write allocate)
            rasr_val |= (1 << 17) | (1 << 16); 
        }

        if (execute_never) {
            rasr_val |= (1 << 28); // Bit 28: XN (Execute Never) 严禁执行命令，防止缓冲区溢出攻击！
        }

        *reg_rasr = rasr_val;
    }

    // ─────────────────────────────────────────────────────────────────────
    // update_user_sandbox_verified — Multi-layer validated sandbox update.
    //
    // Checks (in order):
    //   1. CRC32 integrity of the descriptor (catches in-memory tampering)
    //   2. size_pow2 range [5, 17]  (32 B … 128 KB)
    //   3. Base address alignment to region size
    //
    // On any failure the MPU region is left unchanged and KERNEL_ASSERT fires.
    // Called from mpu_switch_sandbox() in interrupts.cpp (PendSV context).
    // ─────────────────────────────────────────────────────────────────────
    void update_user_sandbox_verified(const SandboxDescriptor& desc) noexcept {
        // 1. CRC32 integrity
        KERNEL_ASSERT(desc.is_valid(), "MPU: SandboxDescriptor CRC32 mismatch");

        // 2. size_pow2 range (32 B = 2^5 … 128 KB = 2^17)
        KERNEL_ASSERT(desc.size_pow2 >= 5u && desc.size_pow2 <= 17u,
                      "MPU: size_pow2 out of range [5..17]");

        // 3. Alignment: base_addr must be a multiple of region size
        const uintptr_t align_mask =
            (static_cast<uintptr_t>(1) << desc.size_pow2) - 1u;
        KERNEL_ASSERT((desc.stack_base & align_mask) == 0u,
                      "MPU: stack_base misaligned to region size");

        // All checks passed — program Region 7
        configure_region(7, desc.stack_base, desc.size_pow2,
                         AP_ALL_RW, /*execute_never=*/true, /*is_device=*/false);
    }

    // Compatibility wrapper for callers that have already validated parameters.
    // 动态更新用户任务的专属沙盒区域（在进程上下文切换 PendSV 时被迅速调用）
    void update_user_sandbox(uintptr_t stack_base, uint8_t size_power_of_2) noexcept {
        // 使用 Region 7 作为当前运行用户的动态栈沙盒以避免与可能的 Region 2 冲突
        configure_region(7, stack_base, size_power_of_2, AP_ALL_RW, true, false);
    }
};

#endif
