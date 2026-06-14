#ifndef __TRACKING_H
#define __TRACKING_H

#include <stdint.h>
#include "esp_s3_i2c.h"

#define SERVO_LIMIT     600     /* ±60° (0.1°), 硬限幅 */

/* 追踪状态 */
typedef struct {
    int16_t pan;        /* 当前PAN角度 (0.1°) */
    int16_t tilt;       /* 当前TILT角度 (0.1°) */
    uint8_t enabled;    /* 追踪模式开关 */
    uint8_t i2c_ok;     /* I2C 通信状态: 0=失败 2=正常 */
} Tracking_t;

extern Tracking_t g_tracking;

void Tracking_Init(void);
void Tracking_Update(void);
void Tracking_Toggle(void);

#endif /* __TRACKING_H */
