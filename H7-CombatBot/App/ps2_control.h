#ifndef __PS2_CONTROL_H
#define __PS2_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ps2.h"

void PS2_DriveControl(const PS2_Joystick_t *js);

#ifdef __cplusplus
}
#endif

#endif /* __PS2_CONTROL_H */
