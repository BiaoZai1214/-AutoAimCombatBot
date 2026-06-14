/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BT_CS_Pin GPIO_PIN_4
#define BT_CS_GPIO_Port GPIOD
#define W5500_RST_Pin GPIO_PIN_4
#define W5500_RST_GPIO_Port GPIOE
#define W5500_CS_Pin GPIO_PIN_14
#define W5500_CS_GPIO_Port GPIOG
#define LED_B_Pin GPIO_PIN_13
#define LED_B_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */
#define BT_CS_Pin GPIO_PIN_4
#define BT_CS_GPIO_Port GPIOD

#define W5500_CS_Pin GPIO_PIN_14
#define W5500_CS_GPIO_Port GPIOG

#define W5500_INT_Pin GPIO_PIN_3
#define W5500_INT_GPIO_Port GPIOE
#define W5500_RST_Pin GPIO_PIN_4
#define W5500_RST_GPIO_Port GPIOE

/* 电机方向控制 — 用户重映射 */
#define M1_IN1_Pin       GPIO_PIN_3
#define M1_IN1_GPIO_Port GPIOD
#define M1_IN2_Pin       GPIO_PIN_0
#define M1_IN2_GPIO_Port GPIOI
#define M2_IN1_Pin       GPIO_PIN_12
#define M2_IN1_GPIO_Port GPIOG
#define M2_IN2_Pin       GPIO_PIN_13
#define M2_IN2_GPIO_Port GPIOG
#define M3_IN1_Pin       GPIO_PIN_11
#define M3_IN1_GPIO_Port GPIOB
#define M3_IN2_Pin       GPIO_PIN_10
#define M3_IN2_GPIO_Port GPIOB
#define M4_IN1_Pin       GPIO_PIN_14
#define M4_IN1_GPIO_Port GPIOH
#define M4_IN2_Pin       GPIO_PIN_11
#define M4_IN2_GPIO_Port GPIOC
#define STBY_Pin         GPIO_PIN_7
#define STBY_GPIO_Port   GPIOC

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
