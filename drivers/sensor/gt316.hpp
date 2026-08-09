// gt316.hpp — GT316 单点触控驱动桩
// 真实硬件通过 I2C1 通讯（地址 0x14），GPIO 中断引脚为 PIN_TOUCH_INT。

#ifndef AURORA_DRIVER_GT316_HPP
#define AURORA_DRIVER_GT316_HPP

#include <stdint.h>

enum class GestureType : uint8_t {
    NONE = 0,
    TAP,
    DOUBLE_TAP,
    LONG_PRESS,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    SWIPE_UP,
    SWIPE_DOWN
};

class Gt316Driver {
public:
    static Gt316Driver& instance() {
        static Gt316Driver drv;
        return drv;
    }

    bool init() {
        initialized_ = true;
        return true;
    }

    // 轮询触控芯片，返回最新手势（无手势返回 NONE）
    GestureType poll_gesture() {
        if (!initialized_) return GestureType::NONE;
        // 仿真：始终返回 NONE，真实硬件通过 GPIO 中断 + I2C 读取坐标
        return GestureType::NONE;
    }

private:
    Gt316Driver() : initialized_(false) {}
    bool initialized_;
};

#endif // AURORA_DRIVER_GT316_HPP
