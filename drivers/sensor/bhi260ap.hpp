// bhi260ap.hpp — BHI260AP 6 轴加速度计驱动桩
// 真实硬件通过 I2C1 通讯，提供步数和三轴加速度数据。

#ifndef AURORA_DRIVER_BHI260AP_HPP
#define AURORA_DRIVER_BHI260AP_HPP

#include <stdint.h>

class Bhi260apDriver {
public:
    static Bhi260apDriver& instance() {
        static Bhi260apDriver drv;
        return drv;
    }

    bool init() {
        initialized_ = true;
        return true;
    }

    // 返回累计步数
    uint32_t get_steps() const {
        if (!initialized_) return 0;
        return steps_;
    }

    // 每 40ms 调用的后台更新（模拟 FIFO 读取）
    void fetch() {
        if (!initialized_) return;
        steps_ += 1;  // 仿真：每 40ms 产生一步（缓慢行走）
        fetch_count_++;
    }

private:
    Bhi260apDriver() : initialized_(false), steps_(0), fetch_count_(0) {}
    bool initialized_;
    uint32_t steps_;
    uint32_t fetch_count_;
};

#endif // AURORA_DRIVER_BHI260AP_HPP
