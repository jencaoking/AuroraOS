#include "uart.h"
#include "config.h"
#include "interrupts.hpp"
#include "task.hpp"
#include "memory.hpp"
#include "softbus.hpp"
#include "mutex.hpp"

extern "C" {
    extern uint32_t _heap_start;
    extern uint32_t _heap_end;
}

// 实例化一个全局互斥锁，保护串口
Mutex uart_mutex;

// 改进版的带锁打印
void safe_print(const char* msg) {
    uart_mutex.lock();
    uart_puts(msg);
    uart_mutex.unlock();
}

extern "C" void bus_daemon_task(void) {
    while (1) {
        SoftBus::instance().poll();
        
        // 优雅休眠：让出 CPU 10 个 Tick (10ms)，不再死占算力
        Scheduler::instance().sleep(10);
    }
}

extern "C" void app_task(void) {
    while (1) {
        safe_print("[App Task] Running background job...\n");
        
        // 模拟耗时任务完成，进入深度休眠 2000ms (2秒)
        Scheduler::instance().sleep(2000);
    }
}

extern "C" void kernel_main(void) {
    uart_init();
    KernelHeap::instance().init(&_heap_start, &_heap_end);
    SoftBus::instance().init();

    // 修改 SoftBus 的回调，使用安全打印
    SoftBus::instance().register_service("PING", [](const char* payload) {
        safe_print(">> [RPC Exec] PING matched! Sending PONG...\n");
        
        uart_mutex.lock();
        SoftBus::instance().send_request("PONG", payload);
        uart_mutex.unlock();
    });

    SoftBus::instance().register_service("SYSINFO", [](const char* payload) {
        (void)payload;
        safe_print(">> [RPC Exec] Fetching system info...\n");
        uart_mutex.lock();
        SoftBus::instance().send_request("INFO", "auroraOS_v0.1_Active");
        uart_mutex.unlock();
    });

    safe_print("\n========================================\n");
    safe_print(" auroraOS Upgraded: Sleep & Mutex Active\n");
    safe_print("========================================\n");

    Scheduler& sched = Scheduler::instance();
    sched.init();

    uint32_t* daemon_stack = new uint32_t[256];
    uint32_t* app_stack = new uint32_t[256];
    
    sched.create_task(bus_daemon_task, daemon_stack, 256 * sizeof(uint32_t));
    sched.create_task(app_task, app_stack, 256 * sizeof(uint32_t));

    g_current_tcb_ptr = sched.get_current_tcb();

    volatile uint32_t* syst_ctrl = reinterpret_cast<volatile uint32_t*>(0xE000E010);
    volatile uint32_t* syst_load = reinterpret_cast<volatile uint32_t*>(0xE000E014);
    *syst_load = (SYSCLK_FREQ / 1000) - 1; // 1ms Tick
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
