#ifndef __LCD_FONT_H
#define __LCD_FONT_H

#include <stdint.h>
#include "string.h"

/* ASCII 字体结构体 */
typedef struct ASCIIFont {
  uint8_t h;
  uint8_t w;
  uint8_t *chars;
} ASCIIFont;

extern const ASCIIFont afont12x8;
extern const ASCIIFont afont18x12;
extern const ASCIIFont afont24x16;

/* pFONT 结构体（当前项目原有格式，兼容保留） */
typedef struct {
	uint16_t Width;       // 字体宽度（像素）
	uint16_t Height;      // 字体高度（像素）
	const uint8_t *pTable; // 字模数据指针
	uint16_t Sizes;       // 单个字符字模字节数
	uint16_t Table_Rows;  // 字库行数（仅中文字体使用）
} pFONT;

/* pFONT 字体变量声明 */
extern const pFONT ASCII_Font24;
extern const pFONT ASCII_Font18x12;



/**
 * @brief 字体结构体
 * @note  字库前4字节存储utf8编码 剩余字节存储字模数据
 * @note  字库数据可以使用波特律动LED取模助手生成(https://led.baud-dance.com)
 */
typedef struct Font {
  uint8_t h;              // 字高度
  uint8_t w;              // 字宽度
  const uint8_t *chars;   // 字库 字库前4字节存储utf8编码 剩余字节存储字模数据
  uint8_t len;            // 字库长度 超过256则请改为uint16_t
  const ASCIIFont *ascii; // 缺省ASCII字体 当字库中没有对应字符且需要显示ASCII字符时使用
} Font;

extern const Font font32x32;

/**
 * @brief 图片结构体
 * @note  图片数据可以使用波特律动LED取模助手生成(https://led.baud-dance.com)
 */
typedef struct Image {
  uint8_t w;           // 图片宽度
  uint8_t h;           // 图片高度
  const uint8_t *data; // 图片数据
} Image;

extern const Image qqImg; 


#endif 
