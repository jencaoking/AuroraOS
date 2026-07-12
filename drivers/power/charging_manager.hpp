#ifndef AURORA_CHARGING_MANAGER_HPP
#define AURORA_CHARGING_MANAGER_HPP

#include <stdint.h>

// 充电状态机
enum class ChargeState : uint8_t {
    DISCHARGING,    // 电池供电
    PRE_CHARGE,     // 预充电 (低电压保护)
    FAST_CHARGE,    // 恒流/恒压快充
    CHARGE_DONE,    // 充满电
    FAULT           // 充电异常 (过温/过压)
};

// ========================================================
// 抽象电池/充电IC驱动接口
// ========================================================
class BatteryDriver {
public:
    virtual ~BatteryDriver() = default;
    virtual bool init() = 0;
    virtual uint16_t read_voltage_mv() = 0;
    virtual bool is_vbus_plugged() = 0;
    virtual ChargeState get_charge_state() = 0;
};

// ========================================================
// 模拟电池驱动 (供测试与 QEMU 使用)
// ========================================================
class MockBatteryDriver : public BatteryDriver {
private:
    uint16_t voltage_mv_;
    bool vbus_plugged_;
    ChargeState state_;

public:
    MockBatteryDriver() : voltage_mv_(3800), vbus_plugged_(false), state_(ChargeState::DISCHARGING) {}

    bool init() override { return true; }
    uint16_t read_voltage_mv() override { return voltage_mv_; }
    bool is_vbus_plugged() override { return vbus_plugged_; }
    ChargeState get_charge_state() override { return state_; }

    // 测试专用辅助函数
    void set_voltage(uint16_t mv) { voltage_mv_ = mv; }
    void set_plugged(bool plugged) {
        vbus_plugged_ = plugged;
        if (!plugged) {
            state_ = ChargeState::DISCHARGING;
        } else if (state_ == ChargeState::DISCHARGING) {
            state_ = ChargeState::FAST_CHARGE;
        }
    }
    void set_state(ChargeState state) { state_ = state; }
};

// ========================================================
// 充电与电量管理器
// ========================================================
class ChargingManager {
private:
    BatteryDriver* driver_;
    MockBatteryDriver default_mock_driver_;

    uint32_t poll_ticks_;
    static constexpr uint32_t POLL_INTERVAL_TICKS = 1000; // 每 1 秒轮询一次 (假设 1ms tick)

    uint16_t current_voltage_mv_;
    uint8_t  current_soc_; // State of Charge (0-100%)
    bool     is_plugged_;
    bool     just_plugged_in_;   // VBUS 插入上升沿
    bool     just_unplugged_;    // VBUS 拔出下降沿
    ChargeState charge_state_;

    ChargingManager() : 
        driver_(&default_mock_driver_), poll_ticks_(0), 
        current_voltage_mv_(0), current_soc_(0), 
        is_plugged_(false), just_plugged_in_(false), just_unplugged_(false),
        charge_state_(ChargeState::DISCHARGING) 
    {
        driver_->init();
        update_battery_status(); // 初始化时拉取一次
    }

    // 查表法计算电池电量百分比 (3.0V - 4.2V 锂电池放电曲线简化)
    uint8_t calculate_soc(uint16_t voltage_mv) {
        if (voltage_mv >= 4150) return 100;
        if (voltage_mv <= 3300) return 0; // 3.3V 视为空电强制关机阈值

        // 简单的分段线性插值 (实际中可根据电池具体放电曲线精调)
        if (voltage_mv > 3800) {
            // 3800mV ~ 4150mV -> 50% ~ 100%
            return 50 + ((voltage_mv - 3800) * 50) / 350;
        } else {
            // 3300mV ~ 3800mV -> 0% ~ 50%
            return ((voltage_mv - 3300) * 50) / 500;
        }
    }

    void update_battery_status() {
        if (!driver_) return;

        current_voltage_mv_ = driver_->read_voltage_mv();
        current_soc_ = calculate_soc(current_voltage_mv_);
        
        bool newly_plugged = driver_->is_vbus_plugged();
        
        // 边缘检测
        just_plugged_in_ = (!is_plugged_ && newly_plugged);
        just_unplugged_  = (is_plugged_ && !newly_plugged);
        
        is_plugged_ = newly_plugged;
        charge_state_ = driver_->get_charge_state();
    }

public:
    // Rule of Five (禁用拷贝)
    ChargingManager(const ChargingManager&) = delete;
    ChargingManager& operator=(const ChargingManager&) = delete;

    static ChargingManager& instance() {
        static ChargingManager manager;
        return manager;
    }

    // 允许注入真实的硬件驱动
    void set_driver(BatteryDriver* driver) {
        if (driver) {
            driver_ = driver;
            driver_->init();
            update_battery_status();
        }
    }

    // 系统心跳级联调用
    void on_tick(uint32_t delta_ticks) {
        poll_ticks_ += delta_ticks;
        just_plugged_in_ = false;
        just_unplugged_  = false;

        if (poll_ticks_ >= POLL_INTERVAL_TICKS) {
            poll_ticks_ = 0;
            update_battery_status();
        }
    }

    // ========================================================
    // 公开查询接口
    // ========================================================
    uint8_t get_soc() const { return current_soc_; }
    uint16_t get_voltage_mv() const { return current_voltage_mv_; }
    bool is_plugged() const { return is_plugged_; }
    bool has_just_plugged() const { return just_plugged_in_; }
    bool has_just_unplugged() const { return just_unplugged_; }
    ChargeState get_charge_state() const { return charge_state_; }

    // 用于提供给 PowerManager 的电量极度危险警告信号 (如低于 5%)
    bool is_critical_low() const {
        return current_soc_ < 5 && !is_plugged_;
    }

    // 获取内部 Mock 驱动指针 (供单元测试使用)
    MockBatteryDriver* get_mock_driver() { return &default_mock_driver_; }
};

#endif // AURORA_CHARGING_MANAGER_HPP
