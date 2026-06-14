#ifndef __SERVO_H
#define __SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tim.h"

#define SERVO_PAN   0   /* 云台左右 (PA2, TIM15_CH1) */
#define SERVO_TILT  1   /* 云台上下 (PA3, TIM15_CH2) */

void    Servo_Init(void);
void    Servo_SetAngle(uint8_t servo, int16_t angle, uint16_t time_ms);
int16_t Servo_GetAngle(uint8_t servo);   /* 读当前角度 (0.1°) */
void    Servo_Tick(void);              /* 20ms 轨迹步进 + 写 CCR */
void    Servo_Release(uint8_t servo);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_H */
