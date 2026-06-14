#include "robot.h"
#include "encoder.h"
#include "tb6612.h"
#include "pid.h"
#include "tim.h"

// # 全局变量
Robot_Wheel_t    R_Wheel[4];  // A（左前） B（右前） C（左后） D（右后）
Robot_Velocity_t R_Vel;

// # PID 参数（增量式）
#define KP   1000
#define KI   800
#define KD   0
#define PID_RATE_HZ    50.0f
#define PWM_MAX        4199

/* 每个脉冲对应轮子的线位移 (m/cnt) */
#define WHEEL_SCALE  (PI * WHEEL_DIAMETER / ENCODER_RES)

/* 有效轮距对角线 (m) */
#define MEC_RADIUS   ((WHEEL_BASE / 2.0f) + (ACLE_BASE / 2.0f))

// # PID 状态
static PID_t    speed_pid[4];
static int16_t  pwm_out[4] = {0, 0, 0, 0};

/* 初始化 */
void Robot_Init(void)
{
    for (int i = 0; i < 4; i++) {
        R_Wheel[i].RT  = 0;
        R_Wheel[i].TG  = 0;
        R_Wheel[i].PWM = 0;
        pwm_out[i]     = 0;
        PID_Init(&speed_pid[i], KP, KI, KD, PWM_MAX, -PWM_MAX);
    }
    R_Vel.TG_IX = R_Vel.TG_IY = R_Vel.TG_IW = 0;
    R_Vel.RT_IX = R_Vel.RT_IY = R_Vel.RT_IW = 0;
    R_Vel.TG_FX = R_Vel.TG_FY = R_Vel.TG_FW = 0;
    R_Vel.RT_FX = R_Vel.RT_FY = R_Vel.RT_FW = 0;

    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);
    for (int i = 0; i < 4; i++) Encoder_ClearCounter(i);

    TB6612_Init();
}

/* 读取编码器 → 轮子实际速度 RT */
static void ReadEncoders(void)
{
    int16_t enc[4];
    enc[0] = Encoder_GetCounter(ENCODER_A); Encoder_ClearCounter(ENCODER_A);
    enc[1] = Encoder_GetCounter(ENCODER_B); Encoder_ClearCounter(ENCODER_B);
    enc[2] = Encoder_GetCounter(ENCODER_C); Encoder_ClearCounter(ENCODER_C);
    enc[3] = Encoder_GetCounter(ENCODER_D); Encoder_ClearCounter(ENCODER_D);

    /* 滑动平均滤波 (窗口5)，平滑编码器测速噪声 */
    static int16_t enc_buf[4][5] = {{0}};
    static uint8_t enc_idx = 0;

    for (int i = 0; i < 4; i++) {
        enc_buf[i][enc_idx] = enc[i];
        int32_t sum = 0;
        for (int k = 0; k < 5; k++) sum += enc_buf[i][k];
        R_Wheel[i].RT = (float)sum / 5.0f * WHEEL_SCALE * PID_RATE_HZ;
    }
    enc_idx = (enc_idx + 1) % 5;
}

/* 开机自检：四轮同时转动 1s，验证电机驱动 + 编码器 */
void Robot_SelfTest(void)
{
    TB6612_SetMotorSpeed(2500, 2500, -2500, -2500);
    HAL_Delay(1000);
    TB6612_SetMotorSpeed(0, 0, 0, 0);
    HAL_Delay(200);

    // # 编码器清零
	for (int i = 0; i < 4; i++)
        Encoder_ClearCounter(i);
}

/* 设置目标速度 (mm/s 0.001rad/s) */
void Robot_SetTarget(int16_t vx, int16_t vy, int16_t vw)
{
    if (vx >  VX_LIMIT) vx =  VX_LIMIT;
    if (vx < -VX_LIMIT) vx = -VX_LIMIT;
    if (vy >  VY_LIMIT) vy =  VY_LIMIT;
    if (vy < -VY_LIMIT) vy = -VY_LIMIT;
    if (vw >  VW_LIMIT) vw =  VW_LIMIT;
    if (vw < -VW_LIMIT) vw = -VW_LIMIT;

    R_Vel.TG_IX = vx;
    R_Vel.TG_IY = vy;
    R_Vel.TG_IW = vw;
    R_Vel.TG_FX = vx / 1000.0f;
    R_Vel.TG_FY = vy / 1000.0f;
    R_Vel.TG_FW = vw / 1000.0f;
}

/* 目标速度限幅 */
static void ClampTarget(void)
{
    int16_t tgx = R_Vel.TG_IX, tgy = R_Vel.TG_IY, tgw = R_Vel.TG_IW;
    if (tgx >  VX_LIMIT) tgx =  VX_LIMIT;
    if (tgx < -VX_LIMIT) tgx = -VX_LIMIT;
    if (tgy >  VY_LIMIT) tgy =  VY_LIMIT;
    if (tgy < -VY_LIMIT) tgy = -VY_LIMIT;
    if (tgw >  VW_LIMIT) tgw =  VW_LIMIT;
    if (tgw < -VW_LIMIT) tgw = -VW_LIMIT;
    R_Vel.TG_FX = tgx / 1000.0f;
    R_Vel.TG_FY = tgy / 1000.0f;
    R_Vel.TG_FW = tgw / 1000.0f;
}

/* 逆运动学: 底盘目标速度 → 各轮目标速度 */
static void InverseKinematics(void)
{
    R_Wheel[0].TG = R_Vel.TG_FX - R_Vel.TG_FY - R_Vel.TG_FW * MEC_RADIUS;
    R_Wheel[1].TG = R_Vel.TG_FX + R_Vel.TG_FY + R_Vel.TG_FW * MEC_RADIUS;
    R_Wheel[2].TG = R_Vel.TG_FX + R_Vel.TG_FY - R_Vel.TG_FW * MEC_RADIUS;
    R_Wheel[3].TG = R_Vel.TG_FX - R_Vel.TG_FY + R_Vel.TG_FW * MEC_RADIUS;
}

// 正运动学: 轮速 → 底盘实际速度 
static void ForwardKinematics(void)
{
    R_Vel.RT_FX = ( R_Wheel[0].RT + R_Wheel[1].RT + R_Wheel[2].RT + R_Wheel[3].RT) / 4.0f;
    R_Vel.RT_FY = (-R_Wheel[0].RT + R_Wheel[1].RT + R_Wheel[2].RT - R_Wheel[3].RT) / 4.0f;
    R_Vel.RT_FW = (-R_Wheel[0].RT + R_Wheel[1].RT - R_Wheel[2].RT + R_Wheel[3].RT) / (4.0f * MEC_RADIUS);
    R_Vel.RT_IX = (int16_t)(R_Vel.RT_FX * 1000);
    R_Vel.RT_IY = (int16_t)(R_Vel.RT_FY * 1000);
    R_Vel.RT_IW = (int16_t)(R_Vel.RT_FW * 1000);
}

// PID 速度环：增量式 + 停转刹车
static void SpeedLoop_Update(void)
{
    for (int i = 0; i < 4; i++) {
        // TG=0 → 跳过 PID，多级刹车
        if (R_Wheel[i].TG == 0.0f) {
            if      (pwm_out[i] >  400) pwm_out[i] -= 300;
            else if (pwm_out[i] < -400) pwm_out[i] += 300;
            else if (pwm_out[i] >  100) pwm_out[i] -= 80;
            else if (pwm_out[i] < -100) pwm_out[i] += 80;
            else                        pwm_out[i]  = 0;
            PID_Reset(&speed_pid[i]);
        } else {
            PID_Update(&speed_pid[i], R_Wheel[i].TG, R_Wheel[i].RT);
            pwm_out[i] = (int16_t)speed_pid[i].out;
        }
        R_Wheel[i].PWM = pwm_out[i];
    }

    /* 输出到电机 (前正后负, O形分布) */
    TB6612_SetMotorSpeed(R_Wheel[0].PWM,  R_Wheel[1].PWM,
                        -R_Wheel[2].PWM, -R_Wheel[3].PWM);
}

// # 复位 (TG + 编码器 + PID) == 
void Robot_Stop(void)
{
    R_Vel.TG_IX = R_Vel.TG_IY = R_Vel.TG_IW = 0;
    R_Vel.TG_FX = R_Vel.TG_FY = R_Vel.TG_FW = 0;
    for (int i = 0; i < 4; i++) {
        R_Wheel[i].TG  = 0;
        R_Wheel[i].PWM = 0;
        pwm_out[i]     = 0;
        PID_Reset(&speed_pid[i]);
        Encoder_ClearCounter(i);
    }
    TB6612_SetMotorSpeed(0, 0, 0, 0);
}

/* 麦轮运动学入口（每 20ms 调用一次） */
void Robot_Kinematics(void)
{
    ReadEncoders();
    ForwardKinematics();

    ClampTarget();
    InverseKinematics();
    SpeedLoop_Update();
}


