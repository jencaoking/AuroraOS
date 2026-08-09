// gh3026.hpp — GH3026 PPG 心率传感器驱动桩
// 真实硬件通过 I2C1 通讯（地址 0x28），QEMU 仿真时返回模拟数据。

#ifndef AURORA_DRIVER_GH3026_HPP
#define AURORA_DRIVER_GH3026_HPP

#include <stdint.h>

class Gh3026Driver {
public:
    static Gh3026Driver& instance() {
        static Gh3026Driver drv;
        return drv;
    }

    bool init() {
        // 真实硬件：通过 I2C1 发送初始化序列
        // write_reg(0x02, 0x01);  // 启动连续采样模式
        initialized_ = true;
        return true;
    }

    // 读取最新心率值（BPM），若传感器无新数据返回 0
    uint32_t read_bpm() {
        if (!initialized_) return 0;
        // 仿真：在 60–100 BPM 之间正弦波动
        sample_counter_++;
        return 60u + (sample_counter_ % 41u);
    }

private:
    Gh3026Driver() : initialized_(false), sample_counter_(0) {}
    bool initialized_;
    uint32_t sample_counter_;
};

#endif // AURORA_DRIVER_GH3026_HPP
