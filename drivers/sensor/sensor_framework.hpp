#ifndef AURORA_SENSOR_FRAMEWORK_HPP
#define AURORA_SENSOR_FRAMEWORK_HPP

#include <stdint.h>
#include "device.hpp"

// 传感器通道类型 (借鉴 Zephyr sensor_channel)
enum class SensorChannel {
    ACCEL_X,
    ACCEL_Y,
    ACCEL_Z,
    HEART_RATE,
    STEP_COUNT,
    BATTERY_LEVEL
};

// 统一的传感器数据值结构
struct SensorValue {
    int32_t val1; // 整数部分
    int32_t val2; // 小数部分 (百万分之一)
};

// 传感器基类
class SensorDevice : public Device {
public:
    SensorDevice(const char* name) : Device(name, DeviceType::Char) {}

    // 核心 API：触发硬件进行一次最新采样
    virtual int sample_fetch() = 0;
    
    // 核心 API：获取指定通道的缓存数据
    virtual int channel_get(SensorChannel chan, SensorValue* val) = 0;
};

// ========================================================
// 模拟心率与计步传感器 (如 HRS3300)
// ========================================================
class HealthSensor : public SensorDevice {
private:
    int32_t current_hr_;
    int32_t current_steps_;

public:
    HealthSensor(const char* name) : SensorDevice(name), current_hr_(75), current_steps_(1000) {}

    int sample_fetch() override {
        // 物理硬件：通过 I2C 发送采样指令并读取 FIFO
        // 这里在仿真中引入一些随机波动
        current_hr_ = 70 + (current_steps_ % 15);
        current_steps_ += 2;
        return 0;
    }

    int channel_get(SensorChannel chan, SensorValue* val) override {
        if (chan == SensorChannel::HEART_RATE) {
            val->val1 = current_hr_;
            val->val2 = 0;
            return 0;
        } else if (chan == SensorChannel::STEP_COUNT) {
            val->val1 = current_steps_;
            val->val2 = 0;
            return 0;
        }
        return -1;
    }
};

#endif
