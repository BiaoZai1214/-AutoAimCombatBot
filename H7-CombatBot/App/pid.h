#ifndef __PID_H
#define __PID_H

#include <stdint.h>

/* 增量式 PID 控制器
 * 输出增量: Δout = Kp*(e-e1) + Ki*e + Kd*(e-2*e1+e2)
 * 当前输出: out += Δout
 */
typedef struct {
    float kp;
    float ki;
    float kd;

    float err;       /* e[n]   */
    float err_last;  /* e[n-1] */
    float err_prev;  /* e[n-2] */

    float out;
    float out_max;
    float out_min;
} PID_t;

void  PID_Init(PID_t *pid, float kp, float ki, float kd,
               float out_max, float out_min);
void  PID_Reset(PID_t *pid);
float PID_Update(PID_t *pid, float target, float actual);

#endif
