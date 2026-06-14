#include "servo.h"

/* PWM 参数：50Hz，500~2500us 对应 ±90° */
#define PULSE_MID   1500
#define ANGLE_MAX   900
#define DEADBAND    3       /* 死区 3us (≈0.27°)，追踪层已做 10px 大死区 */

/* 轨迹规划：每 tick 最大步进 3° (0.1° 单位) = 150°/s */
#define STEP_MAX    30

static int16_t  g_angle_target[2]  = {0, 0};   /* 目标角度 (0.1°) */
static int16_t  g_angle_current[2] = {0, 0};   /* 当前角度 (0.1°) */
static uint16_t g_last_us[2]       = {PULSE_MID, PULSE_MID};

void Servo_Init(void)
{
    __HAL_RCC_TIM15_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF4_TIM15;
    HAL_GPIO_Init(GPIOA, &gpio);

    TIM15->PSC = 199;
    TIM15->ARR = 19999;
    TIM15->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2
                 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM15->CCR1 = PULSE_MID;
    TIM15->CCR2 = PULSE_MID;
    TIM15->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
    TIM15->BDTR |= TIM_BDTR_MOE;
    TIM15->EGR = TIM_EGR_UG;
    TIM15->CR1 |= TIM_CR1_CEN;

    /* TIM7: 20ms 中断 */
    __HAL_RCC_TIM7_CLK_ENABLE();
    TIM7->PSC = 399;
    TIM7->ARR = 9999;
    TIM7->DIER |= TIM_DIER_UIE;
    TIM7->SR  = ~TIM_SR_UIF;
    TIM7->CR1 |= TIM_CR1_CEN;
    NVIC_EnableIRQ(TIM7_IRQn);
    NVIC_SetPriority(TIM7_IRQn, 5);
}

/* 读取当前角度 (0.1°) */
int16_t Servo_GetAngle(uint8_t servo)
{
    if (servo > 1) return 0;
    return g_angle_current[servo];
}

/* 设置舵机目标角度（Servo_Tick 逐步逼近） */
void Servo_SetAngle(uint8_t servo, int16_t angle, uint16_t time_ms)
{
    (void)time_ms;
    if (servo > 1) return;
    if (angle >  ANGLE_MAX) angle =  ANGLE_MAX;
    if (angle < -ANGLE_MAX) angle = -ANGLE_MAX;
    g_angle_target[servo] = angle;
}

/* TIM7 中断 20ms 调用：轨迹步进 → 写 CCR */
void Servo_Tick(void)
{
    for (int i = 0; i < 2; i++) {
        int16_t diff = g_angle_target[i] - g_angle_current[i];

        if (diff >  STEP_MAX) diff =  STEP_MAX;
        if (diff < -STEP_MAX) diff = -STEP_MAX;
        if (diff == 0) continue;

        g_angle_current[i] += diff;

		// 转换系数 = 脉宽变化/角度变化 = 1000 / 90° = 10/9
        uint16_t us = (uint16_t)(PULSE_MID + (int32_t)g_angle_current[i] * 10 / 9);

        uint16_t change = (us > g_last_us[i]) ? (us - g_last_us[i])
                                              : (g_last_us[i] - us);
        if (change < DEADBAND) continue;

        g_last_us[i] = us;
        if (i == SERVO_PAN)
            TIM15->CCR1 = us;
        else
            TIM15->CCR2 = us;
    }
}
