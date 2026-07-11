#ifndef MUTEX_HPP
#define MUTEX_HPP

#include "task.hpp"
#include "syscall.hpp"

class Mutex {
private:
    volatile bool locked_ = false;

public:
    void lock() {
        while (true) {
            // 使用 GCC 内置原子操作，在特权和非特权模式下均安全生效
            if (!__sync_lock_test_and_set(&locked_, true)) {
                return; // 成功获取锁
            }
            
            // 锁被占用，非特权态无法直接写 ICSR 触发 PendSV，
            // 必须通过系统调用安全地让出 CPU
            sys_yield();
        }
    }

    void unlock() {
        __sync_lock_release(&locked_);
    }
};

// CP.20: Use RAII, never plain lock()/unlock()
struct LockGuard {
    Mutex& m_;
    explicit LockGuard(Mutex& m) : m_(m) { m_.lock(); }
    ~LockGuard() { m_.unlock(); }
    
    // Non-copyable
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

#endif
