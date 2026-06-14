#include "encoder.h"

int16_t Encoder_GetCounter(uint8_t idx)
{
    int16_t cnt = 0;
    switch (idx) {
        case ENCODER_A: cnt = (int16_t)__HAL_TIM_GET_COUNTER(&htim2); break;
        case ENCODER_B: cnt = (int16_t)__HAL_TIM_GET_COUNTER(&htim3); break;
        case ENCODER_C: cnt = (int16_t)__HAL_TIM_GET_COUNTER(&htim4); break;
        case ENCODER_D: cnt = (int16_t)__HAL_TIM_GET_COUNTER(&htim5); break;
    }
    return cnt;
}

void Encoder_ClearCounter(uint8_t idx)
{
    switch (idx) {
        case ENCODER_A: __HAL_TIM_SET_COUNTER(&htim2, 0); break;
        case ENCODER_B: __HAL_TIM_SET_COUNTER(&htim3, 0); break;
        case ENCODER_C: __HAL_TIM_SET_COUNTER(&htim4, 0); break;
        case ENCODER_D: __HAL_TIM_SET_COUNTER(&htim5, 0); break;
    }
}
