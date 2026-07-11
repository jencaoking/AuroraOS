#include "interrupts.hpp"
#include "uart.h"
#include "task.hpp"

// 供汇编读取的两个全局 TCB 指针
extern "C" {
    TaskControlBlock* g_current_tcb_ptr = nullptr;
    TaskControlBlock* g_next_tcb_ptr = nullptr;
}

static uint32_t tick_count = 0;

InterruptManager& InterruptManager::instance()
{
    static InterruptManager mgr;
    return mgr;
}

void InterruptManager::init()
{
    for (int i = 0; i < MAX_IRQ; i++)
        handlers_[i] = nullptr;
}

void InterruptManager::register_handler(int irq, InterruptHandler handler)
{
    if (irq >= 0 && irq < MAX_IRQ)
        handlers_[irq] = handler;
}

void InterruptManager::unregister_handler(int irq)
{
    if (irq >= 0 && irq < MAX_IRQ)
        handlers_[irq] = nullptr;
}

void InterruptManager::handle(InterruptFrame* frame)
{
    (void)frame;
}

extern "C" {

void SVC_Handler(InterruptFrame* frame)
{
    InterruptManager::instance().handle(frame);
}

void SysTick_Handler(void) {
    tick_count++;
    Scheduler& sched = Scheduler::instance();
    
    // 更新所有处于 Sleeping 状态的线程
    sched.tick_update();
    
    // 每 10ms 强制进行一次时间片轮转（如果线程没被阻塞的话）
    if (tick_count % 10 == 0) {
        sched.schedule(); 
    }
}

}

