#include "uart.h"
#include "config.h"
#include "interrupts.hpp"
#include "task.hpp"
#include "memory.hpp"
#include "softbus.hpp"

extern "C" {
    extern uint32_t _heap_start;
    extern uint32_t _heap_end;
}

// ==========================================
// 软总线守护线程 (Daemon Task)
// ==========================================
extern "C" void bus_daemon_task(void) {
    while (1) {
        // 持续剥离和解析总线上的协议帧
        SoftBus::instance().poll();
        
        // 防止独占 CPU，简单延时让出（后续可以替换为线程 sleep 机制）
        for (volatile int i = 0; i < 5000; i++);
    }
}

// 模拟另外一个前台业务线程
extern "C" void app_task(void) {
    while (1) {
        // 这里可以执行其他业务逻辑...
        for (volatile int i = 0; i < 1000000; i++);
    }
}

extern "C" void kernel_main(void) {
    uart_init();
    KernelHeap::instance().init(&_heap_start, &_heap_end);
    
    uart_puts("\n========================================\n");
    uart_puts("   auroraOS SoftBus Subsystem Booting...\n");
    uart_puts("========================================\n");

    // 1. 初始化软总线
    SoftBus::instance().init();

    // 2. 将系统能力注册为“微服务”暴露给总线
    SoftBus::instance().register_service("PING", [](const char* payload) {
        uart_puts(">> [RPC Exec] PING matched! Sending PONG...\n");
        SoftBus::instance().send_request("PONG", payload);
    });

    SoftBus::instance().register_service("SYSINFO", [](const char* payload) {
        (void)payload;
        uart_puts(">> [RPC Exec] Fetching system info...\n");
        SoftBus::instance().send_request("INFO", "auroraOS_v0.1_Active");
    });

    uart_puts("[SoftBus]: RPC Services [PING], [SYSINFO] mounted.\n");

    // 3. 动态分配栈空间并起飞多线程
    Scheduler& sched = Scheduler::instance();
    sched.init();

    uint32_t* daemon_stack = new uint32_t[256];
    uint32_t* app_stack = new uint32_t[256];
    
    sched.create_task(bus_daemon_task, daemon_stack, 256 * sizeof(uint32_t));
    sched.create_task(app_task, app_stack, 256 * sizeof(uint32_t));

    g_current_tcb_ptr = sched.get_current_tcb();

    // 启动系统滴答时钟并切入线程环境
    volatile uint32_t* syst_ctrl = reinterpret_cast<volatile uint32_t*>(0xE000E010);
    volatile uint32_t* syst_load = reinterpret_cast<volatile uint32_t*>(0xE000E014);
    *syst_load = (SYSCLK_FREQ / 1000) - 1;
    *syst_ctrl = (1 << 2) | (1 << 1) | (1 << 0);

    __asm__ volatile (
        "msr psp, %0\n\t"
        "mov r0, #2\n\t"
        "msr control, r0\n\t"
        "isb\n\t"
        "cpsie i\n\t"
        "bl bus_daemon_task\n\t"
        : : "r"(g_current_tcb_ptr->stack_ptr) : "r0", "memory"
    );

    while (1) {}
}
