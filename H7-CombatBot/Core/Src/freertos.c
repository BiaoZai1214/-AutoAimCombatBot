/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    freertos.c
  * @brief   FreeRTOS 任务注册 + 启动
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "robot.h"
#include "ui.h"
#include "eth.h"
#include "http_server.h"
#include "ps2_control.h"
#include "tracking.h"
#include "esp_s3_i2c.h"
#include "adc.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
#define CONTROL_TASK_STACK      512
#define CONTROL_TASK_PRIORITY   ( tskIDLE_PRIORITY + 2 )

#define W5500_TASK_STACK        512
#define W5500_TASK_PRIORITY     ( tskIDLE_PRIORITY + 2 )

#define LED_TASK_STACK          512
#define LED_TASK_PRIORITY       ( tskIDLE_PRIORITY + 1 )
/* USER CODE END Variables */

/* Function prototypes -------------------------------------------------------*/
/* USER CODE BEGIN Function_Prototypes */

static void Control_Task(void *pvParam);
static void W5500_Task(void *pvParam);
static void LED_Task(void *pvParam);

/* USER CODE END Function_Prototypes */

/* USER CODE BEGIN Application */

void freertos_start(void)
{
    xTaskCreate(Control_Task,
                "Control",
                CONTROL_TASK_STACK,
                NULL,
                CONTROL_TASK_PRIORITY,
                NULL);

    xTaskCreate(W5500_Task,
                "W5500",
                W5500_TASK_STACK,
                NULL,
                W5500_TASK_PRIORITY,
                NULL);

    xTaskCreate(LED_Task,
                "LED",
                LED_TASK_STACK,
                NULL,
                LED_TASK_PRIORITY,
                NULL);

    vTaskStartScheduler();
}

// # Control 任务: 底盘 + 自瞄 + LCD显示
static void Control_Task(void *pvParam)
{
    // # 开机
    Robot_SelfTest();       // 电机自检 + 编码器清零
    ESP_S3_Init();           // 视觉通信 I2C
    Tracking_Init();        // 追踪状态机
    UI_ShowTarget();        // LCD: 目标追踪界面

    uint32_t lcd_last = 0;  /* LCD 刷新计时 */

    // # 主循环 20ms
    PS2_Joystick_t js = {0};
    for (;;) {
        if (PS2_Scan(&js) == HAL_OK)
            PS2_DriveControl(&js);  // # 摇杆 -> 目标速度
        else
            Robot_Stop();

        Robot_Kinematics();         // # 运动学 + PID + PWM

        Tracking_Update();          // # 自瞄: I2C 读 ESP32 -> 写舵机

        /* LCD 每 200ms 刷新一次（避免 SPI 占用过高） */
        if (HAL_GetTick() - lcd_last >= 200) {
            lcd_last = HAL_GetTick();
            UI_ShowTarget();
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// # W5500 任务: 以太网初始化 -> HTTP 服务
static void W5500_Task(void *pvParam)
{
    ETH_Init();  // SPI + W5500 配置
    vTaskDelay(pdMS_TO_TICKS(1500));

    for (;;) {
        HTTP_Server_Run();  // 处理一次 HTTP 连接（内部 delay）
    }
}

// # LED 任务: 状态指示
//   快闪(200ms): I2C 失败（ESP32 未响应）
//   慢闪(2s):   I2C 成功，收到目标数据
//   常灭:       追踪关闭
static void LED_Task(void *pvParam)
{
    uint32_t batt_last = 0;
    for (;;) {
        if (!g_tracking.enabled) {
            HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);

        int dly;
        if      (g_tracking.i2c_ok == 2) dly = 2000;
        else if (g_tracking.i2c_ok == 0) dly = 200;
        else                             dly = 1000;

        if (HAL_GetTick() - batt_last >= 60000) {
            batt_last = HAL_GetTick();
            uint16_t adc = ADC_ReadBattery();
            float volt = (float)adc * 3.3f / 4095.0f * 18.0f;
            printf("Battery: %d mV\r\n", (int)(volt * 1000));
        }

        vTaskDelay(pdMS_TO_TICKS(dly));
    }
}
/* USER CODE END Application */
