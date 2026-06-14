#ifndef __ESP_S3_I2C_H
#define __ESP_S3_I2C_H

#include "main.h"

/* I2C 从机地址（ESP32-S3） */
#define ESP_S3_I2C_ADDR     0x52

/* 寄存器命令 */
#define ESP_S3_CMD_ANGLE    0x01    /* 读角度: 5字节 pan_lo,pan_hi,tilt_lo,tilt_hi,id */

/* 角度数据结构 (与 ESP32 协议匹配) */
typedef struct {
    int16_t pan;        /* PAN 角度 (0.1°) */
    int16_t tilt;       /* TILT 角度 (0.1°) */
    uint8_t id;         /* 0=丢失, 2=锁定 */
} ESP_AngleData_t;

/* ── 函数接口 ── */
HAL_StatusTypeDef ESP_S3_Init(void);
HAL_StatusTypeDef ESP_S3_ReadAngle(ESP_AngleData_t *angle);

#endif /* __ESP_S3_I2C_H */
