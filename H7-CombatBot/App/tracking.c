/**
 * 从 ESP32 接收角度 → 直驱舵机
 * ESP32 做 P 控制 + EMA 滤波，STM32 只负责读 I2C + 驱舵机
 * 保护层: id=0 冻结 | ±60° 硬限幅
 */

#include "tracking.h"
#include "servo.h"

Tracking_t g_tracking = {0};

void Tracking_Init(void)
{
    g_tracking.pan     = 0;
    g_tracking.tilt    = 0;
    g_tracking.enabled = 1;
    g_tracking.i2c_ok  = 0;

    Servo_SetAngle(SERVO_PAN,  0, 0);
    Servo_SetAngle(SERVO_TILT, 0, 0);
}

void Tracking_Toggle(void)
{
    g_tracking.enabled = !g_tracking.enabled;

    /* 关闭追踪时舵机归中 */
    if (!g_tracking.enabled) {
        Servo_SetAngle(SERVO_PAN,  0, 0);
        Servo_SetAngle(SERVO_TILT, 0, 0);
    }
}

void Tracking_Update(void)
{
    if (!g_tracking.enabled)
        return;

    /* I2C 退避：连续失败后降低轮询频率 */
    static uint8_t  fail_cnt = 0;
    static uint32_t last_try = 0;

    uint32_t interval = (fail_cnt < 3)  ? 20  :
                        (fail_cnt < 10) ? 200 : 500;

    if (HAL_GetTick() - last_try < interval)
        return;
    last_try = HAL_GetTick();

    ESP_AngleData_t angle;
    if (ESP_S3_ReadAngle(&angle) != HAL_OK) {
        fail_cnt++;
        g_tracking.i2c_ok = 0;
        if (fail_cnt > 150) fail_cnt = 0;
        return;
    }

    fail_cnt = 0;
    g_tracking.i2c_ok = 2;

    /* 目标丢失: 不更新角度, 舵机停在原位 */
    if (angle.id == 0)
        return;

    /* 硬限幅 ±60° */
    if (angle.pan >  SERVO_LIMIT) angle.pan =  SERVO_LIMIT;
    if (angle.pan < -SERVO_LIMIT) angle.pan = -SERVO_LIMIT;
    if (angle.tilt >  SERVO_LIMIT) angle.tilt =  SERVO_LIMIT;
    if (angle.tilt < -SERVO_LIMIT) angle.tilt = -SERVO_LIMIT;

    /* 直驱舵机 — DS3218 内部 PID 全权处理运动 */
    g_tracking.pan  = angle.pan;
    g_tracking.tilt = angle.tilt;
    Servo_SetAngle(SERVO_PAN,  angle.pan, 0);
    Servo_SetAngle(SERVO_TILT, angle.tilt, 0);
}
