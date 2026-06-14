#include "pid.h"

void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float out_max, float out_min)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->out_max = out_max;
    pid->out_min = out_min;
    PID_Reset(pid);
}

void PID_Reset(PID_t *pid)
{
    pid->err = 0;
    pid->err_last = 0;
    pid->err_prev = 0;
    pid->out = 0;
}

// # 增量式PID（速度环）
float PID_Update(PID_t *pid, float target, float actual)
{
    /* 更新误差历史 */
    pid->err_prev = pid->err_last;
    pid->err_last = pid->err;
    pid->err = target - actual;

    float err      = pid->err;
    float err_last = pid->err_last;
    float err_prev = pid->err_prev;

    /* 增量式 PID: out += Kp*(err-err_last) + Ki*err + Kd*(err-2*err_last+err_prev) */
    pid->out += pid->kp * (err - err_last)
              + pid->ki * err
              + pid->kd * (err - 2.0f * err_last + err_prev);

    /* 输出限幅 */
    if (pid->out > pid->out_max) pid->out = pid->out_max;
    if (pid->out < pid->out_min) pid->out = pid->out_min;

    return pid->out;
}
