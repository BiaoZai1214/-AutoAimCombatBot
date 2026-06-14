#ifndef __ROBOT_H
#define __ROBOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// # 机器人底盘参数
#define WHEEL_DIAMETER    0.080f    /* 轮直径 (m) */
#define WHEEL_BASE        0.182f    /* 左右轮距 (m) */
#define ACLE_BASE         0.124f    /* 前后轴距 (m) */
#define ENCODER_LINE      13        /* 编码器每圈线数 */
#define GEAR_RATIO        30        /* 电机减速比 */
#define PI                3.1416f

/* 编码器分辨率: 线数 × 减速比 × 4倍频 = 13*30*4 = 1560 */
#define ENCODER_RES       1560.0f

// # 速度限幅
#define VX_LIMIT   1500    /*   X 速度上限 (mm/s), 即 1.5 m/s */
#define VY_LIMIT   1200    /*   Y 速度上限 (mm/s) */
#define VW_LIMIT   6280    /* 旋转速度上限 (0.001 rad/s), 每秒1圈 */

// # 运动学结构体

/* 单轮数据 */
typedef struct {
    float   RT;    /* 实时速度 (m/s) */
    float   TG;    /* 目标速度 (m/s) */
    int16_t PWM;   /* PWM输出值 (-4199~+4199) */
} Robot_Wheel_t;

/* 底盘速度 */
typedef struct {
	/* X方向目标 (mm/s) */
    int16_t TG_IX;   /* X方向目标 (mm/s) */
    int16_t TG_IY;   /* Y方向目标 (mm/s) */
    int16_t TG_IW;   /* Yaw旋转目标 (0.001rad/s) */
	
	int16_t RT_IX;   /* X实时 (mm/s) */
    int16_t RT_IY;   /* Y实时 (mm/s) */
    int16_t RT_IW;   /* Yaw实时 (0.001rad/s) */
	
    float   TG_FX;   /* X目标 (m/s) */
    float   TG_FY;   /* Y目标 (m/s) */
    float   TG_FW;   /* Yaw目标 (rad/s) */

    float   RT_FX;   /* X实际 (m/s) */
    float   RT_FY;   /* Y实际 (m/s) */
    float   RT_FW;   /* Yaw实际 (rad/s) */
} Robot_Velocity_t;

// # 全局变量
extern Robot_Wheel_t    R_Wheel[4];   /* A左前 B右前 C左后 D右后 */
extern Robot_Velocity_t R_Vel;

// # 函数接口
void Robot_Init(void);
void Robot_SelfTest(void);      /* 开机自检：电机短转 + 清零编码器 */
void Robot_Kinematics(void);    /* 麦轮运动学 + PID + PWM 输出 */
void Robot_SetTarget(int16_t vx, int16_t vy, int16_t vw);
void Robot_Stop(void);          /* 停电机 + 复位 TG/编码器/PD 状态 */

#ifdef __cplusplus
}
#endif

#endif /* __ROBOT_H */
