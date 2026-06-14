#include "ui.h"
#include "lcd_spi_200.h"
#include "tracking.h"
#include <stdio.h>
#include <string.h>

/* 主界面：显示 Driving Mode */
void UI_ShowMain(void)
{
    LCD_Clear(BLACK);
    LCD_PrintASCIIString(24, 130, "Driving Mode", &ASCII_Font24, WHITE);
    LCD_Update();
}

/* 目标追踪界面：显示角度和连接状态 */
void UI_ShowTarget(void)
{
    char buf[24];
    uint16_t y = CONTENT_Y;

    LCD_ClearRect(CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, BLACK);

    /* ── 标题栏 ── */
    LCD_PrintASCIIString(40, y, "Gimbal Tracking", &ASCII_Font24, YELLOW);
    y += 32;

    LCD_DrawLine_H(10, y, 220, GRAY);
    y += 8;

    /* ── 连接状态 ── */
    if (g_tracking.i2c_ok == 2) {
        LCD_PrintASCIIString(10, y, "I2C: OK", &ASCII_Font18x12, GREEN);
    } else {
        LCD_PrintASCIIString(10, y, "I2C: FAIL", &ASCII_Font18x12, RED);
    }
    y += 22;

    /* ── 目标状态 ── */
    if (g_tracking.i2c_ok == 2) {
        LCD_PrintASCIIString(10, y, "State: LOCKED", &ASCII_Font18x12, GREEN);
    } else {
        LCD_PrintASCIIString(10, y, "State: SEARCHING...", &ASCII_Font18x12, YELLOW);
    }
    y += 26;

    LCD_DrawLine_H(10, y, 220, GRAY);
    y += 8;

    /* ── 舵机角度 ── */
    sprintf(buf, "PAN:  %4d", g_tracking.pan);
    LCD_PrintASCIIString(10, y, buf, &ASCII_Font18x12, CYAN);
    y += 22;

    sprintf(buf, "TILT: %4d", g_tracking.tilt);
    LCD_PrintASCIIString(10, y, buf, &ASCII_Font18x12, CYAN);
    y += 28;

    /* ── 底部状态栏 ── */
    LCD_DrawLine_H(10, y, 220, GRAY);
    y += 4;
    if (g_tracking.enabled) {
        LCD_PrintASCIIString(50, y, "[Tracking ON]", &ASCII_Font18x12, GREEN);
    } else {
        LCD_PrintASCIIString(50, y, "[Tracking OFF]", &ASCII_Font18x12, RED);
    }

    LCD_Update();
}
