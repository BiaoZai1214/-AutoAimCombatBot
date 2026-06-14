# AutoAimCombatBot — 自瞄步战车

基于 STM32H750 + ESP32-S3 双芯片架构的麦轮战斗机器人。STM32 负责底盘控制与舵机云台，ESP32-S3 负责视觉追踪，I2C 实时通信协同。

## 项目结构

```
├── H7-CombatBot/     ← STM32H750 主控固件（Keil MDK + FreeRTOS）
├── R5-tracker/       ← ESP32-S3 视觉追踪固件（ESP-IDF + ESP-DL）
└── README.md
```

---

## H7-CombatBot — 底盘 + 云台主控

**MCU**: STM32H750XBH6 (Cortex-M7, 400MHz)  
**RTOS**: FreeRTOS v10.3.1 (heap_4)  
**IDE**: Keil MDK V5.27 + STM32CubeMX

### 功能
- **全向移动** — 麦轮 O 型布局，增量式 PD 独立四轮速度闭环
- **PS2 遥控** — 双摇杆 + DPAD + 肩键操控
- **二轴云台** — DS3218 舵机 ×2（PAN/TILT），50Hz PWM，TIM7 轨迹步进平滑运动
- **自动追踪** — 通过 I2C 接收 ESP32-S3 角度指令，SELECT 键切换手动/自动模式
- **以太网 Web** — W5500 HTTP Server，网页查看状态
- **LCD 仪表盘** — ST7789 240×320 RGB565，显示驾驶模式、电池电量

### 硬件

| 器件 | 型号 | 接口 |
|------|------|------|
| 主控 | STM32H750XBH6 | — |
| 电机驱动 | TB6612 ×2 | TIM1 CH1-4 PWM |
| 编码器 | 增量式 ×4 | TIM2-5 正交 |
| 遥控 | PS2 无线手柄 | SPI2 |
| 视觉模块 | ESP32-S3-CAM | I2C1 (PB8/PB9) |
| 舵机 | DS3218 ×2 | TIM15 CH1/CH2 |
| LCD | ST7789 240×320 | SPI5 |
| 以太网 | W5500 | SPI4 |

### 构建

在 Keil MDK 中打开 `H7-CombatBot/MDK-ARM/Project.uvprojx` 编译即可。固件体积约 Code 49KB, ZI 223KB。

### 操作

| 按键 | 功能 |
|------|------|
| 左摇杆 | 前进/后退 + 横移 |
| 右摇杆 | 旋转 |
| DPAD | 四方向满幅行走 |
| L2/R2 | PAN 云台控制 |
| L1/R1 | TILT 云台控制 |
| SELECT | 切换 手动/自动追踪 |

---

## R5-Tracker — 视觉追踪模组

**MCU**: ESP32-S3  
**SDK**: ESP-IDF  
**Camera**: OV3660 (RGB565)  
**算法**: ESP-DL HSV 颜色检测 + PD 追踪

### 功能
- 摄像头采集 OV3660 RGB565 图像
- ESP-DL HSV 查找表颜色识别（默认追踪绿色物体）
- Blob 扫描找最大连通域
- 增量式 PD 控制器计算目标角度
- I2C Slave (0x52) 向 STM32 发送 PAN/TILT 角度指令

### 构建

```bash
cd R5-tracker
idf.py build
idf.py -p COMx flash
```

### I2C 通信协议

ESP32-S3 作为 I2C Slave (地址 0x52)，STM32 发 1 字节命令 → 读 4 字节返回，超时 20ms。

| STM32 写 | ESP 返回 4 字节 | 说明 |
|----------|-----------------|------|
| `0x10` | pan_hi, pan_lo, tilt_hi, tilt_lo | 舵机角度 (int16, 0.01°, ±6000) |

---

## 系统数据流

```
PS2手柄 ──SPI2──→ ps2_control.c ──→ robot.c (运动学+PID) ──→ tb6612.c ──→ 电机
                   │                    ↑
                   ├─→ servo.c          │
ESP-S3 ──I2C1──→ tracking.c ──→ servo.c │
编码器 ──TIM2-5──→ encoder.c ──→ robot.c┘
LCD ──SPI5──→ ui.c
W5500 ──SPI4──→ http_server.c
```

---

## 学习文档

`H7-CombatBot/docs/` 中有三份学习指南：
- **项目总览** — 架构与模块关系
- **底盘控制** — 麦轮运动学 + PID 调试
- **自瞄云台** — 舵机控制 + 视觉追踪

## 更多

详细项目说明请访问博客：[https://biaozai1214.github.io/Bzone-blog/](https://biaozai1214.github.io/Bzone-blog/)

## 许可

MIT License
