#ifndef __spi_lcd
#define __spi_lcd

#include "stm32h7xx_hal.h"
#include "usart.h"
#include "lcd_fonts.h"

/*========================================= 屏幕参数 =========================================*/

#define LCD_Width     240		// LCD 宽度
#define LCD_Height    320		// LCD 高度

/* 内容显示区域（避开屏幕圆角，上下留边距） */
#define CONTENT_X       0
#define CONTENT_Y       20
#define CONTENT_W       LCD_Width           // 240
#define CONTENT_H       (LCD_Height - 40)   // 280, Y范围 20~300

/* 显示方向（由 LCD_SetDirection() 设置） */
#define	Direction_H				0	// 横屏
#define	Direction_H_Flip	   	1	// 横屏翻转
#define	Direction_V				2	// 竖屏
#define	Direction_V_Flip	   	3	// 竖屏翻转

/* 数字显示模式（前导补零或空格） */
#define  Fill_Zero  0		// 补零
#define  Fill_Space 1		// 补空格

/*========================================= RGB565 颜色定义 =========================================
 * 参考 E:\Code\Register\LCD 项目风格，颜色值直接定义为 RGB565 16位格式
 * 绘图函数直接接收 uint16_t color 参数，无需 24位→16位 转换
 */

#define	WHITE         	 0xFFFF
#define	BLACK         	 0x0000
#define	BLUE           	 0x001F
#define	GREEN         	 0x07E0
#define	RED           	 0xF800
#define	NAVY			 0x000F
#define	DGREEN			 0x03E0
#define	DCYAN			 0x03EF
#define	MAROON			 0x7800
#define	PURPLE			 0x780F
#define	OLIVE			 0x7BE0
#define	LIGHT_GREY		 0xC618
#define	DARK_GREY		 0x7BEF
#define	BLUE2			 0x051D
#define	BLUE3			 0x0018
#define	BLUE4			 0x0010
#define	YELLOW        	 0xFFE0
#define	MAGENTA       	 0xF81F
#define	CYAN          	 0x07FF
#define	BROWN 			 0xBC40
#define	BRRED 			 0xFC07
#define	GRAY  			 0x8430
#define	DARKBLUE      	 0x01CF
#define	LIGHTBLUE      	 0x7D7C
#define	GRAYBLUE       	 0x5458
#define	LIGHTGREEN     	 0x841F
#define	LGRAY 			 0xC618
#define	LGRAYBLUE        0xA651
#define	LBBLUE           0x2B12


// # 变量声明
extern uint16_t LCD_FrameBuf[LCD_Width * LCD_Height];  /* 帧缓冲 */

// # 函数声明

// # 初始化
void  SPI_LCD_Init(void);      // 液晶屏及SPI初始化（原名保留兼容）
void  LCD_Init(void);          // 统一初始化入口

// # 帧缓冲同步（保留）
void  LCD_Update(void);        // 整个帧缓冲刷到屏幕（DMA）
void  LCD_UpdateArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h); // 刷新指定区域

// # 清屏
void  LCD_Clear(uint16_t color);           // 清屏（填色 + 刷新）
void  LCD_ClearRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color); // 区域清空（仅帧缓冲）

// # 基础绘图
void  LCD_SetPixel(uint16_t x, uint16_t y, uint16_t color);        // 画点
void  LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color); // 填充矩形
void  LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color); // 画线
void  LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t h, uint16_t color);   // 垂直直线
void  LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t w, uint16_t color);   // 水平直线
void  LCD_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color); // 空心矩形
void  LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);   // 空心圆
void  LCD_DrawEllipse(int x, int y, int r1, int r2, uint16_t color);        // 空心椭圆
void  LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);   // 实心圆

// # 文本显示
void  LCD_PrintASCIIChar(uint16_t x, uint16_t y, uint8_t c, const pFONT *font, uint16_t color);
void  LCD_PrintASCIIString(uint16_t x, uint16_t y, char *str, const pFONT *font, uint16_t color);
void  LCD_PrintString(uint16_t x, uint16_t y, char *str, const pFONT *font, uint16_t color); // 中文+ASCII混排

// # 数字显示
void  LCD_ShowNumMode(uint8_t mode);
void  LCD_DisplayNumber(uint16_t x, uint16_t y, int32_t num, uint8_t len, uint16_t color);
void  LCD_DisplayDecimals(uint16_t x, uint16_t y, double num, uint8_t len, uint8_t dec, uint16_t color);

// # 方向/坐标
void  LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);	// 设置地址窗口
void  LCD_SetDirection(uint8_t direction);  	                          // 显示方向

// # 批量拷贝
void  LCD_CopyBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *DataBuff);


// # 引脚定义

#define LCD_Backlight_PIN			GPIO_PIN_6
#define	LCD_Backlight_PORT			GPIOH
#define GPIO_LDC_Backlight_CLK_ENABLE  __HAL_RCC_GPIOH_CLK_ENABLE()

#define	LCD_Backlight_OFF		HAL_GPIO_WritePin(LCD_Backlight_PORT, LCD_Backlight_PIN, GPIO_PIN_RESET)
#define	LCD_Backlight_ON		HAL_GPIO_WritePin(LCD_Backlight_PORT, LCD_Backlight_PIN, GPIO_PIN_SET)

#define  LCD_DC_PIN				GPIO_PIN_11
#define	LCD_DC_PORT				GPIOJ
#define GPIO_LDC_DC_CLK_ENABLE  __HAL_RCC_GPIOJ_CLK_ENABLE()

#define	LCD_DC_Command		   HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define	LCD_DC_Data		      HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)

#endif //__spi_lcd
