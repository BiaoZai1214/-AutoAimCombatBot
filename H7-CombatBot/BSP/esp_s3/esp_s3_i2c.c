#include "esp_s3_i2c.h"

static I2C_HandleTypeDef hi2c1;

/* I2C1: PB8(SCL) / PB9(SDA) — AF4
 * APB1=100MHz, Timing=0x10D1F8F8 → 100kHz */
HAL_StatusTypeDef ESP_S3_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_OD;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &gpio);

    hi2c1.Instance             = I2C1;
    hi2c1.Init.Timing          = 0x10D1F8F8;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        return HAL_ERROR;

    HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
    return HAL_OK;
}

/* 从 ESP32 读取舵机角度命令
 * 协议: Write 0x01 → Read 5字节 (pan_lo,pan_hi,tilt_lo,tilt_hi,id)
 * pan/tilt 为 int16 LE, id: 2=锁定 0=丢失, 单位 0.1°
 * 带 3 次重试，超时 20ms */
HAL_StatusTypeDef ESP_S3_ReadAngle(ESP_AngleData_t *angle)
{
    uint8_t buf[5];
    uint8_t reg = ESP_S3_CMD_ANGLE;

    for (int retry = 0; retry < 3; retry++) {
        if (HAL_I2C_Master_Transmit(&hi2c1, ESP_S3_I2C_ADDR << 1, &reg, 1, 20) != HAL_OK)
            continue;

        /* 给 ESP32 留 ~500us 准备 TX FIFO */
        for (volatile int d = 0; d < 5000; d++) { __asm volatile("nop"); }

        if (HAL_I2C_Master_Receive(&hi2c1, ESP_S3_I2C_ADDR << 1, buf, 5, 20) == HAL_OK) {
            angle->pan  = (int16_t)(buf[0] | (buf[1] << 8));
            angle->tilt = (int16_t)(buf[2] | (buf[3] << 8));
            angle->id   = buf[4];
            return HAL_OK;
        }
    }

    return HAL_ERROR;
}
