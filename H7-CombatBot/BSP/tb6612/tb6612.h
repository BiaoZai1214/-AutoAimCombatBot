#ifndef __TB6612_H
#define __TB6612_H

#include "main.h"
#include "tim.h"

void TB6612_Init(void);
void TB6612_SetMotorSpeed(int16_t m1, int16_t m2, int16_t m3, int16_t m4);

#endif /* __TB6612_H */
