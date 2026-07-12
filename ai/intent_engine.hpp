#ifndef AURORA_INTENT_ENGINE_HPP
#define AURORA_INTENT_ENGINE_HPP

#include "../drivers/sensor/sensor_framework.hpp"
#include "../kernel/app_lifecycle.hpp"

extern HealthSensor g_health_sensor;

class IntentEngine {
public:
    // 基于传感器规则的决策引擎
    static void process_sensors(AppControlBlock& fitness_app) {
        SensorDevice* sensor = &g_health_sensor;
        if (!sensor) return;

        SensorValue val;
        // 注意：HealthSensor 模拟器目前只实现了 HEART_RATE 和 STEP_COUNT
        // 我们可以复用 STEP_COUNT 作为触发意图的信号
        sensor->channel_get(SensorChannel::STEP_COUNT, &val);

        // 简易意图逻辑：如果步数增加导致变化超过某个阈值，判定为“运动开始”
        // 或者是每 20 步唤醒一次
        if (val.val1 > 0 && (val.val1 % 20 == 0)) { 
            if (fitness_app.state != AppState::FOREGROUND) {
                fitness_app.transition_to(AppState::FOREGROUND);
                
                int fd = open("/dev/uart0", 0);
                if (fd >= 0) {
                    write(fd, "\r\n🤖 [Intent Engine] High activity detected! Promoting Fitness App to FOREGROUND.\r\n", 83);
                    close(fd);
                }
            }
        } else {
            // 否则降级到后台
            if (fitness_app.state == AppState::FOREGROUND && (val.val1 % 20 != 0)) {
                fitness_app.transition_to(AppState::BACKGROUND);
                int fd = open("/dev/uart0", 0);
                if (fd >= 0) {
                    write(fd, "\r\n🤖 [Intent Engine] Activity reduced. Demoting Fitness App to BACKGROUND.\r\n", 76);
                    close(fd);
                }
            }
        }
    }
};

#endif
