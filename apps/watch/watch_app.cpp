#include "watch_app.hpp"
#include "power_manager.hpp"
#include "st7789_driver.hpp"
#include "sensor_framework.hpp"
#include "gesture_recognizer.hpp"
#include "ble_stack.hpp"
#include "font_engine.hpp"

// ========================================================
// 静态全局变量与微型显存池
// ========================================================
static constexpr uint16_t CHUNK_HEIGHT = 64;
static uint16_t render_buffer[192 * CHUNK_HEIGHT];

// 深色系主题常量 (极致降低 AMOLED 功耗)
static constexpr uint16_t COLOR_BG_DARK    = 0x0821; // 深渊黑
static constexpr uint16_t COLOR_TEXT_ACCENT = 0x07E0; // 极光绿
static constexpr uint16_t COLOR_TEXT_MUTED  = 0x8410; // 碳灰

// ========================================================
// 1. GUI 渲染管线接管 (Watch Face)
// ========================================================
void WatchApp::build_watch_face_ui() {
    watch_face_view_ = new UI::ViewGroup(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // 顶部状态栏: 蓝牙和电量
    UI::TextView* ble_text = new UI::TextView(150, 10, "BLE", 0x07E0, 0, 1);
    watch_face_view_->add_child(ble_text);
    
    // 时间显示 (居中)
    time_text_ = new UI::TextView(20, 100, "10:09", 0xFFFF, 0, 4);
    watch_face_view_->add_child(time_text_);

    // 运动健康数据 (底部)
    hr_text_ = new UI::TextView(80, 250, "HR: 0", 0xF800, 0, 2);
    watch_face_view_->add_child(hr_text_);

    steps_text_ = new UI::TextView(80, 300, "STP: 0", 0x07E0, 0, 2);
    watch_face_view_->add_child(steps_text_);
    
    // 进度条圆弧
    UI::ArcProgress* battery_arc = new UI::ArcProgress(96, 400, 40, 85, 0x07E0);
    watch_face_view_->add_child(battery_arc);
}

// ========================================================
// 2. 交互状态路由接管 (7 种手势响应)
// ========================================================
void WatchApp::handle_gesture(GestureType gesture) {
    if (gesture == GestureType::NONE) return;

    // 交互防抖：触发任何手势，系统立即重置熄屏倒计时，保持 Active 状态
    PowerManager::instance().transition_to(PowerState::ACTIVE);

    // 将枚举事件封装为不带坐标的简单手势事件并路由给 UI 框架
    UI::GestureEvent event = {gesture, 0, 0};
    UI::UiManager::instance().dispatch_gesture(event);

    // 全局页面手势拦截
    switch (current_page_) {
        case WatchPage::WATCH_FACE:
            if (gesture == GestureType::SWIPE_DOWN) {
                current_page_ = WatchPage::QUICK_PANEL; // 下拉呼出控制中心
            } else if (gesture == GestureType::SWIPE_LEFT) {
                current_page_ = WatchPage::HEART_RATE; // 左滑进入心率检测页面
            }
            break;
        case WatchPage::HEART_RATE:
            if (gesture == GestureType::SWIPE_RIGHT) {
                current_page_ = WatchPage::WATCH_FACE; // 右滑返回
            }
            break;
        case WatchPage::QUICK_PANEL:
            if (gesture == GestureType::SWIPE_UP) {
                current_page_ = WatchPage::WATCH_FACE; // 上滑返回表盘
            }
            break;
        default:
            break;
    }
}

// ========================================================
// 3. 蓝牙与后台数据流同步接管
// ========================================================
void WatchApp::on_background_tick(uint32_t delta_ticks) {
    // 驱动电源生命周期引擎
    PowerManager::instance().on_tick(delta_ticks);

    // 模拟时间流逝
    static uint32_t ms_accumulator = 0;
    ms_accumulator += delta_ticks;
    if (ms_accumulator >= 60000) { // 每 60 秒 (1分钟)
        ms_accumulator = 0;
        simulated_time_m_++;
        if (simulated_time_m_ >= 60) {
            simulated_time_m_ = 0;
            simulated_time_h_ = (simulated_time_h_ + 1) % 24;
        }
        
        // 更新 UI 时间 (格式 10:09)
        static char time_str[6];
        time_str[0] = (simulated_time_h_ / 10) + '0';
        time_str[1] = (simulated_time_h_ % 10) + '0';
        time_str[2] = ':';
        time_str[3] = (simulated_time_m_ / 10) + '0';
        time_str[4] = (simulated_time_m_ % 10) + '0';
        time_str[5] = '\0';
        if (time_text_) {
            time_text_->set_text(time_str);
        }
    }

    uint32_t current_bpm = 0;
    uint32_t current_steps = SensorManager::instance().get_accel_sensor().get_steps();
    SensorData data;
    if (SensorManager::instance().pop_data(&data) && data.type == SensorType::HEART_RATE) {
        current_bpm = data.payload.bpm;
    }

    // 更新 UI 组件数据
    static char hr_buf[16];
    static char stp_buf[16];
    static uint32_t last_bpm = 0xFFFFFFFF;
    static uint32_t last_steps = 0xFFFFFFFF;

    if (current_bpm != last_bpm && hr_text_) {
        hr_buf[0] = 'H'; hr_buf[1] = 'R'; hr_buf[2] = ':'; hr_buf[3] = ' ';
        hr_buf[4] = (current_bpm / 100) + '0';
        hr_buf[5] = ((current_bpm / 10) % 10) + '0';
        hr_buf[6] = (current_bpm % 10) + '0';
        hr_buf[7] = '\0';
        hr_text_->set_text(hr_buf);
        last_bpm = current_bpm;
    }

    if (current_steps != last_steps && steps_text_) {
        stp_buf[0] = 'S'; stp_buf[1] = 'T'; stp_buf[2] = 'P'; stp_buf[3] = ':'; stp_buf[4] = ' ';
        stp_buf[5] = (current_steps / 1000) + '0';
        stp_buf[6] = ((current_steps / 100) % 10) + '0';
        stp_buf[7] = ((current_steps / 10) % 10) + '0';
        stp_buf[8] = (current_steps % 10) + '0';
        stp_buf[9] = '\0';
        steps_text_->set_text(stp_buf);
        last_steps = current_steps;
    }

    // 蓝牙 GATT Server 数据同步
    if (BleManager::instance().get_state() == BleConnectionState::CONNECTED) {
        static uint32_t sync_throttle = 0;
        sync_throttle += delta_ticks;
        
        // 限制蓝牙同步频率为 1Hz，防止射频芯片过热并节省电量
        if (sync_throttle >= 1000) {
            sync_throttle = 0;
            if (current_bpm > 0) {
                BleManager::instance().update_heart_rate(static_cast<uint8_t>(current_bpm));
            }
            BleManager::instance().update_battery_level(85);
        }
    }
}
