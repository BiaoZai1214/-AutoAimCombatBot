#ifndef __ENCODER_H
#define __ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tim.h"

/* 编码器索引 */
#define ENCODER_A  0   /* 左前电机编码器 (TIM2: PA15/PB3) */
#define ENCODER_B  1   /* 右前电机编码器 (TIM3: PB4/PB5) */
#define ENCODER_C  2   /* 左后电机编码器 (TIM4: PD12/PD13) */
#define ENCODER_D  3   /* 右后电机编码器 (TIM5: PA0/PH11) */

int16_t Encoder_GetCounter(uint8_t idx);
void    Encoder_ClearCounter(uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_H */
