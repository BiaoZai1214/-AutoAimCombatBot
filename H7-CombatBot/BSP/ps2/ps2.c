#include "ps2.h"
#include "spi.h"

static const uint8_t ps2_cmd[9] = {
  0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t ps2_data[9];

void PS2_Init(void)
{
  HAL_GPIO_WritePin(BT_CS_GPIO_Port, BT_CS_Pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef PS2_Scan(PS2_Joystick_t *joystick)
{
  uint8_t i;
  HAL_StatusTypeDef status = HAL_OK;

  if (joystick == NULL)
  {
    return HAL_ERROR;
  }

  HAL_GPIO_WritePin(BT_CS_GPIO_Port, BT_CS_Pin, GPIO_PIN_RESET);

  for (i = 0; i < sizeof(ps2_cmd); i++)
  {
    uint8_t tx = ps2_cmd[i];
    uint8_t rx = 0x00;

    status = HAL_SPI_TransmitReceive(&hspi2, &tx, &rx, 1, 10);
    ps2_data[i] = rx;

    if (status != HAL_OK)
    {
      break;
    }
  }

  HAL_GPIO_WritePin(BT_CS_GPIO_Port, BT_CS_Pin, GPIO_PIN_SET);

  if (status != HAL_OK)
  {
    return status;
  }

  joystick->mode = ps2_data[1];
  joystick->btn1 = (uint8_t)~ps2_data[3];
  joystick->btn2 = (uint8_t)~ps2_data[4];
  joystick->RJoy_LR = ps2_data[5];
  joystick->RJoy_UD = ps2_data[6];
  joystick->LJoy_LR = ps2_data[7];
  joystick->LJoy_UD = ps2_data[8];

  return HAL_OK;
}
