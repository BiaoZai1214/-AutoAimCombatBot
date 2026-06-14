#ifndef __PS2_H__
#define __PS2_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef struct
{
  uint8_t mode;
  uint8_t btn1;
  uint8_t btn2;
  uint8_t RJoy_LR;
  uint8_t RJoy_UD;
  uint8_t LJoy_LR;
  uint8_t LJoy_UD;
} PS2_Joystick_t;

#define PS2_BTN_SELECT   (1U << 0)
#define PS2_BTN_JOY_R    (1U << 1)
#define PS2_BTN_JOY_L    (1U << 2)
#define PS2_BTN_START    (1U << 3)
#define PS2_BTN_UP       (1U << 4)
#define PS2_BTN_RIGHT    (1U << 5)
#define PS2_BTN_DOWN     (1U << 6)
#define PS2_BTN_LEFT     (1U << 7)

#define PS2_BTN_L2       (1U << 0)
#define PS2_BTN_R2       (1U << 1)
#define PS2_BTN_L1       (1U << 2)
#define PS2_BTN_R1       (1U << 3)
#define PS2_BTN_TRIANGLE (1U << 4)
#define PS2_BTN_CIRCLE   (1U << 5)
#define PS2_BTN_CROSS    (1U << 6)
#define PS2_BTN_SQUARE   (1U << 7)

void PS2_Init(void);
HAL_StatusTypeDef PS2_Scan(PS2_Joystick_t *joystick);

#ifdef __cplusplus
}
#endif

#endif /* __PS2_H__ */
