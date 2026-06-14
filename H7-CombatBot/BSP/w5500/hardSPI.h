#ifndef __HARDSPI_H
#define __HARDSPI_H

#include "main.h"

/* W5500 CS 控制 - PG14 */
#define CS_LOW()     HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH()    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET)

/* W5500 RST 引脚 - 根据实际硬件接线修改 */
#define RST_LOW()    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET)
#define RST_HIGH()   HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET)

/* SPI句柄（使用SPI4）*/
#define W5500_SPI    hspi4
extern SPI_HandleTypeDef W5500_SPI;

void hardSPI_Init(void);
void hardSPI_Start(void);
void hardSPI_Stop(void);
uint8_t hardSPI_SwapByte(uint8_t byte);

#endif
