#include "tb6612.h"

typedef struct {
    GPIO_TypeDef *in1;
    uint16_t      pin1;
    GPIO_TypeDef *in2;
    uint16_t      pin2;
    uint32_t      ch;
} MotorPin_t;

static const MotorPin_t motor[4] = {
    { M1_IN1_GPIO_Port, M1_IN1_Pin, M1_IN2_GPIO_Port, M1_IN2_Pin, TIM_CHANNEL_1 },  /* M1 左前 */
    { M2_IN1_GPIO_Port, M2_IN1_Pin, M2_IN2_GPIO_Port, M2_IN2_Pin, TIM_CHANNEL_2 },  /* M2 右前 */
    { M3_IN1_GPIO_Port, M3_IN1_Pin, M3_IN2_GPIO_Port, M3_IN2_Pin, TIM_CHANNEL_3 },  /* M3 左后 */
    { M4_IN1_GPIO_Port, M4_IN1_Pin, M4_IN2_GPIO_Port, M4_IN2_Pin, TIM_CHANNEL_4 },  /* M4 右后 */
};

void TB6612_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

    for (int i = 0; i < 4; i++) {
        __HAL_TIM_SET_COMPARE(&htim1, motor[i].ch, 0);
        HAL_GPIO_WritePin(motor[i].in1, motor[i].pin1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(motor[i].in2, motor[i].pin2, GPIO_PIN_RESET);
    }
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_SET);
}

/* 电机真值表
IN1  IN2  |  电机
──────────┼───────
 0    0   |  刹车
 1    0   |  正转
 0    1   |  反转
 1    1   |  刹车	*/

// speed: -4199 ~ +4199, 0=刹车
void TB6612_SetMotorSpeed(int16_t m1, int16_t m2, int16_t m3, int16_t m4)
{
    int16_t spd[4] = { m1, m2, m3, m4 };

    for (int i = 0; i < 4; i++) {
        __HAL_TIM_SET_COMPARE(&htim1, motor[i].ch,
                              spd[i] < 0 ? -spd[i] : spd[i]);

        HAL_GPIO_WritePin(motor[i].in1, motor[i].pin1,
                          spd[i] > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(motor[i].in2, motor[i].pin2,
                          spd[i] < 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
