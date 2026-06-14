#include "ps2_control.h"
#include "robot.h"
#include "servo.h"
#include "tracking.h"

static uint8_t g_speed = 12;

// # 手柄有效性校验：全0：未链接； 全FF：数据混乱
static int8_t PS2_IsValid(const PS2_Joystick_t *js)
{
    uint8_t all0  = (js->RJoy_LR == 0 && js->RJoy_UD == 0 &&
                     js->LJoy_LR == 0 && js->LJoy_UD == 0);
    uint8_t allFF = (js->RJoy_LR == 0xFF && js->RJoy_UD == 0xFF &&
                     js->LJoy_LR == 0xFF && js->LJoy_UD == 0xFF);
    return (js->mode == 0x73) && !all0 && !allFF;
}

void PS2_DriveControl(const PS2_Joystick_t *js)
{
	// 数据有效性保护；错误即刹车
    if (!PS2_IsValid(js)) {
        Robot_Stop();
        return;
    }

    #define DEADZONE 6	// # 死区

    // # 左摇杆: 前/左（0） (0x80) 后/右（FF） 
    int16_t rx = (int16_t)0x80 - js->LJoy_UD;
    int16_t ry = (int16_t)js->LJoy_LR - 0x80;
    if (rx > -DEADZONE && rx < DEADZONE) rx = 0;
    if (ry > -DEADZONE && ry < DEADZONE) ry = 0;
	
	/* 右摇杆: 左右旋转 */
    int16_t rw = (int16_t)js->RJoy_LR - 0x80;  
    if (rw > -DEADZONE && rw < DEADZONE) rw = 0;

	/* 十字键: 位置 -> 满幅移动 */
    if (js->btn1 & PS2_BTN_UP)    rx =  128;
    if (js->btn1 & PS2_BTN_DOWN)  rx = -127;
    if (js->btn1 & PS2_BTN_RIGHT) ry =  127;
    if (js->btn1 & PS2_BTN_LEFT)  ry = -128;

    /* 横向补偿 1.3x：麦轮侧移效率低于直行 */
    Robot_SetTarget((int16_t)(g_speed * rx),
                     (int16_t)(g_speed * -ry * 13 / 10),
                     (int16_t)(4 * g_speed * -rw));  /* 翻转右摇杆旋转方向 */

    // # SELECT: 下降沿触发切换追踪（松开才切，防连发）
    {
        static uint8_t prev_sel = 0, cooldown = 0;
        uint8_t cur_sel = (js->btn1 & PS2_BTN_SELECT) ? 1 : 0;
        if (cooldown > 0) cooldown--;
        if (prev_sel && !cur_sel && cooldown == 0) {
            Tracking_Toggle();
            cooldown = 15;  // 300ms 冷却防连发
        }
        prev_sel = cur_sel;
    }

    // # 舵机手动控制（自动追踪模式下跳过）
    if (!g_tracking.enabled) {
        static int16_t pan = 0, tilt = 0;

        typedef struct { uint8_t count, state, lock; } Debounce_t;
        static Debounce_t db[4] = {{0}};
        #define DB_THRESH 3

        static const int8_t step_map[4] = { 12, -12, 12, -12 };

        uint8_t cur = js->btn2 & 0x0F;

        for (int i = 0; i < 4; i++) {
            uint8_t raw = (cur >> i) & 1;

            if (raw == db[i].state) {
                if (db[i].count < DB_THRESH) db[i].count++;
            } else {
                if (db[i].count > 0) {
                    db[i].count--;
                } else {
                    db[i].state = raw;
                }
            }

            if (db[i].state && !db[i].lock) {
                db[i].lock = 1;
                if (i < 2) pan  += step_map[i];
                else       tilt += step_map[i];
            }
            if (db[i].lock) db[i].lock--;
        }

        if (pan  >  900) pan  =  900;
        if (pan  < -900) pan  = -900;
        if (tilt >  900) tilt =  900;
        if (tilt < -900) tilt = -900;

        static int16_t last_pan = 0, last_tilt = 0;
        if (pan != last_pan) {
            Servo_SetAngle(SERVO_PAN,  pan,  0);
            last_pan = pan;
        }
        if (tilt != last_tilt) {
            Servo_SetAngle(SERVO_TILT, tilt, 0);
            last_tilt = tilt;
        }
    }
}
