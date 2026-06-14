#include <stdio.h>
#include <string.h>
#include "lcd_spi_200.h"
#include "main.h"

SPI_HandleTypeDef hspi5;	       /* SPI5 句柄，中断文件 extern 引用 */

#define  LCD_SPI hspi5            // SPI局部宏，方便修改和移植

/* DMA 相关 */
DMA_HandleTypeDef hdma_spi5_tx;
static volatile uint8_t lcd_dma_done = 0;     // DMA传输完成标志

/*
 * 帧缓冲 — 全分辨率 240×320×2 = 153,600 字节
 * 所有绘图操作写入此缓冲，LCD_Update() 统一刷到屏幕
 * 32字节对齐：H7 D-Cache line 为 32 字节，Cache 刷写要求对齐
 */
__ALIGN_BEGIN uint16_t LCD_FrameBuf[LCD_Width * LCD_Height] __ALIGN_END;

/*
 * 临时行缓冲（保留，用于 LCD_UpdateArea 的行拼接）
 * 1024 个字 = 2048 字节，足以容纳一行 240 像素
 */
static uint16_t  LCD_Buff[1024];

/* LCD 全局参数结构体 */
struct
{
	uint8_t  ShowNum_Mode;	  // 数字显示模式
	uint8_t  Direction;		  // 显示方向
	uint16_t Width;           // 屏幕像素长度
	uint16_t Height;          // 屏幕像素宽度
	uint8_t  X_Offset;        // X坐标偏移
	uint8_t  Y_Offset;        // Y坐标偏移
} LCD;

/* 前向声明 */
HAL_StatusTypeDef LCD_SPI_Transmit(SPI_HandleTypeDef *hspi, uint16_t pData, uint32_t Size);
HAL_StatusTypeDef LCD_SPI_TransmitBuffer(SPI_HandleTypeDef *hspi, uint16_t *pData, uint32_t Size);
static void LCD_SPI_DMA_Init(void);
static HAL_StatusTypeDef LCD_SPI_TransmitBuffer_DMA(uint16_t *pData, uint32_t Size);

/****************************************************************************************************************************************
 * 函 数 名:	MX_SPI5_Init
 * 函数功能:	初始化SPI配置
 * 说    明:	使用硬件片选
****************************************************************************************************************************************/

/* SPI5 MspInit — SPI5 不在 CubeMX .ioc 中，手动配置时钟和引脚 */
static void SPI5_MspInit(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_SPI5_CLK_ENABLE();
	__HAL_RCC_GPIOK_CLK_ENABLE();
	__HAL_RCC_GPIOJ_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();

	/* SPI5 SCK(PK0), MOSI(PJ10), NSS(PH5) */
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI5;
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = GPIO_PIN_10;
	HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = GPIO_PIN_5;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	/* LCD 背光 PH6 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	/* 数据/指令选择 PJ11 */
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(SPI5_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(SPI5_IRQn);
}

void MX_SPI5_Init(void)
{
	SPI5_MspInit();

	LCD_SPI.Instance 						= SPI5;
	LCD_SPI.Init.Mode 					= SPI_MODE_MASTER;
	LCD_SPI.Init.Direction 				= SPI_DIRECTION_1LINE;
	LCD_SPI.Init.DataSize 				= SPI_DATASIZE_8BIT;
	LCD_SPI.Init.CLKPolarity 			= SPI_POLARITY_LOW;
	LCD_SPI.Init.CLKPhase 				= SPI_PHASE_1EDGE;
	LCD_SPI.Init.NSS 					= SPI_NSS_HARD_OUTPUT;
	LCD_SPI.Init.BaudRatePrescaler 		= SPI_BAUDRATEPRESCALER_2;
	LCD_SPI.Init.FirstBit				= SPI_FIRSTBIT_MSB;
	LCD_SPI.Init.TIMode 				= SPI_TIMODE_DISABLE;
	LCD_SPI.Init.CRCCalculation			= SPI_CRCCALCULATION_DISABLE;
	LCD_SPI.Init.CRCPolynomial 			= 0x0;
	LCD_SPI.Init.NSSPMode 				= SPI_NSS_PULSE_ENABLE;
	LCD_SPI.Init.NSSPolarity 			= SPI_NSS_POLARITY_LOW;
	LCD_SPI.Init.FifoThreshold 			= SPI_FIFO_THRESHOLD_02DATA;
	LCD_SPI.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	LCD_SPI.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	LCD_SPI.Init.MasterSSIdleness 		= SPI_MASTER_SS_IDLENESS_00CYCLE;
	LCD_SPI.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	LCD_SPI.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
	LCD_SPI.Init.MasterKeepIOState 		= SPI_MASTER_KEEP_IO_STATE_DISABLE;
	LCD_SPI.Init.IOSwap 				= SPI_IO_SWAP_DISABLE;

	HAL_SPI_Init(&LCD_SPI);
	LCD_SPI_DMA_Init();
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_SPI_DMA_Init
 * 函数功能:	初始化 SPI5 TX DMA (DMA1 Stream5)
****************************************************************************************************************************************/

static void LCD_SPI_DMA_Init(void)
{
	__HAL_RCC_DMA1_CLK_ENABLE();

	hdma_spi5_tx.Instance = DMA1_Stream5;
	hdma_spi5_tx.Init.Request = DMA_REQUEST_SPI5_TX;
	hdma_spi5_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
	hdma_spi5_tx.Init.PeriphInc = DMA_PINC_DISABLE;
	hdma_spi5_tx.Init.MemInc = DMA_MINC_ENABLE;
	hdma_spi5_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
	hdma_spi5_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
	hdma_spi5_tx.Init.Mode = DMA_NORMAL;
	hdma_spi5_tx.Init.Priority = DMA_PRIORITY_LOW;
	hdma_spi5_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	hdma_spi5_tx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
	hdma_spi5_tx.Init.MemBurst = DMA_MBURST_SINGLE;
	hdma_spi5_tx.Init.PeriphBurst = DMA_PBURST_SINGLE;

	HAL_DMA_Init(&hdma_spi5_tx);

	__HAL_LINKDMA(&LCD_SPI, hdmatx, hdma_spi5_tx);

	HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

/****************************************************************************************************************************************
 * 函 数 名: LCD_WriteCommand
 * 入口参数: lcd_command - 需要写入的控制指令
 * 函数功能: 向屏幕控制器写入指令
****************************************************************************************************************************************/

void LCD_WriteCommand(uint8_t lcd_command)
{
	LCD_DC_Command;
	HAL_SPI_Transmit(&LCD_SPI, &lcd_command, 1, 1000);
}

/****************************************************************************************************************************************
 * 函 数 名: LCD_WriteData_8bit
 * 入口参数: lcd_data - 8位数据
 * 函数功能: 写入8位数据
****************************************************************************************************************************************/

void LCD_WriteData_8bit(uint8_t lcd_data)
{
	LCD_DC_Data;
	HAL_SPI_Transmit(&LCD_SPI, &lcd_data, 1, 1000);
}

/****************************************************************************************************************************************
 * 函 数 名: LCD_WriteData_16bit
 * 入口参数: lcd_data - 16位数据
 * 函数功能: 写入16位数据（拆分为2个8位发送）
****************************************************************************************************************************************/

void LCD_WriteData_16bit(uint16_t lcd_data)
{
	uint8_t lcd_data_buff[2];
	LCD_DC_Data;

	lcd_data_buff[0] = lcd_data >> 8;
	lcd_data_buff[1] = lcd_data;

	HAL_SPI_Transmit(&LCD_SPI, lcd_data_buff, 2, 1000);
}

/****************************************************************************************************************************************
 * 函 数 名: LCD_WriteBuff
 * 入口参数: DataBuff - 数据区，DataSize - 数据长度（16位字数量）
 * 函数功能: 批量写入数据到屏幕（数据量 ≤32 轮询，>32 DMA）
****************************************************************************************************************************************/

void LCD_WriteBuff(uint16_t *DataBuff, uint16_t DataSize)
{
	if (DataSize > 32)
	{
		LCD_SPI_TransmitBuffer_DMA(DataBuff, DataSize);
	}
	else
	{
		LCD_DC_Data;
		LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;
		HAL_SPI_Init(&LCD_SPI);
		HAL_SPI_Transmit(&LCD_SPI, (uint8_t *)DataBuff, DataSize, 1000);
		LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
		HAL_SPI_Init(&LCD_SPI);
	}
}

/****************************************************************************************************************************************
 * 函 数 名: SPI_LCD_Init  /  LCD_Init
 * 函数功能: 初始化SPI以及屏幕控制器的各种参数，清空帧缓冲并刷到屏幕
****************************************************************************************************************************************/

void SPI_LCD_Init(void)
{
	MX_SPI5_Init();               // 初始化SPI和控制引脚

	HAL_Delay(10);
	LCD_WriteCommand(0x01);       // 软件复位（弥补无硬件 RST 引脚的问题）
	HAL_Delay(120);               // 等待复位完成（数据手册要求 ≥5ms，留余量）

	LCD_WriteCommand(0x36);       // 显存访问控制
	LCD_WriteData_8bit(0x00);     // 从上到下、从左到右，RGB

	LCD_WriteCommand(0x3A);		  // 接口像素格式
	LCD_WriteData_8bit(0x05);     // 16位像素格式

	LCD_WriteCommand(0xB2);
	LCD_WriteData_8bit(0x0C);
	LCD_WriteData_8bit(0x0C);
	LCD_WriteData_8bit(0x00);
	LCD_WriteData_8bit(0x33);
	LCD_WriteData_8bit(0x33);

	LCD_WriteCommand(0xB7);		  // 栅极电压
	LCD_WriteData_8bit(0x35);

	LCD_WriteCommand(0xBB);		  // 公共电压
	LCD_WriteData_8bit(0x19);

	LCD_WriteCommand(0xC0);
	LCD_WriteData_8bit(0x2C);

	LCD_WriteCommand(0xC2);
	LCD_WriteData_8bit(0x01);

	LCD_WriteCommand(0xC3);		  // VRH电压
	LCD_WriteData_8bit(0x12);

	LCD_WriteCommand(0xC4);		  // VDV电压
	LCD_WriteData_8bit(0x20);

	LCD_WriteCommand(0xC6); 	  // 帧率
	LCD_WriteData_8bit(0x0F);

	LCD_WriteCommand(0xD0);		  // 电源控制
	LCD_WriteData_8bit(0xA4);
	LCD_WriteData_8bit(0xA1);

	LCD_WriteCommand(0xE0);       // 正极伽马
	LCD_WriteData_8bit(0xD0);
	LCD_WriteData_8bit(0x04);
	LCD_WriteData_8bit(0x0D);
	LCD_WriteData_8bit(0x11);
	LCD_WriteData_8bit(0x13);
	LCD_WriteData_8bit(0x2B);
	LCD_WriteData_8bit(0x3F);
	LCD_WriteData_8bit(0x54);
	LCD_WriteData_8bit(0x4C);
	LCD_WriteData_8bit(0x18);
	LCD_WriteData_8bit(0x0D);
	LCD_WriteData_8bit(0x0B);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x23);

	LCD_WriteCommand(0xE1);       // 负极伽马
	LCD_WriteData_8bit(0xD0);
	LCD_WriteData_8bit(0x04);
	LCD_WriteData_8bit(0x0C);
	LCD_WriteData_8bit(0x11);
	LCD_WriteData_8bit(0x13);
	LCD_WriteData_8bit(0x2C);
	LCD_WriteData_8bit(0x3F);
	LCD_WriteData_8bit(0x44);
	LCD_WriteData_8bit(0x51);
	LCD_WriteData_8bit(0x2F);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x20);
	LCD_WriteData_8bit(0x23);

	LCD_WriteCommand(0x21);       // 打开反显

	LCD_WriteCommand(0x11);       // 退出休眠
	HAL_Delay(120);

	/* 在开显示之前先把帧缓冲清黑并刷入 GRAM，避免上电首帧显示随机数据 */
	LCD_SetDirection(Direction_V_Flip);
	uint32_t i;
	for (i = 0; i < LCD_Width * LCD_Height; i++)
		LCD_FrameBuf[i] = BLACK;
	LCD_Update();

	LCD_WriteCommand(0x29);       // 打开显示
	HAL_Delay(50);                // 等待显示稳定后再开背光

	/* LCD_SetAsciiFont 已删除，字体改为直接传参 */
	LCD_ShowNumMode(Fill_Zero);

	LCD_Backlight_ON;
}

/* 统一初始化入口 */
void LCD_Init(void)
{
	SPI_LCD_Init();
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_Update
 * 函数功能:	将整个帧缓冲刷到屏幕（使用 DMA，约 20ms @60MHz SPI）
****************************************************************************************************************************************/

/****************************************************************************************************************************************
 * 函 数 名:	LCD_Update
 * 函数功能:	将整个帧缓冲刷到屏幕（使用 DMA，约 20ms @60MHz SPI）
****************************************************************************************************************************************/

void LCD_Update(void)
{
	uint32_t pixels = LCD.Width * LCD.Height;

	LCD_SetAddress(0, 0, LCD.Width - 1, LCD.Height - 1);

	/*
	 * 全帧刷新使用阻塞 SPI 传输（LCD_SPI_TransmitBuffer），
	 * 它设置 TSIZE=0 绕过硬件长度限制，支持任意大小传输，
	 * 不存在 DMA 的 65535 上限问题，也避免拆包时 CS 拉高中断 GRAM 写入。
	 */
	LCD_DC_Data;
	SCB_CleanDCache_by_Addr((uint32_t *)LCD_FrameBuf, pixels * sizeof(uint16_t));

	LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;
	HAL_SPI_Init(&LCD_SPI);

	LCD_SPI_TransmitBuffer(&LCD_SPI, LCD_FrameBuf, pixels);

	LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
	HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_UpdateArea
 * 函数功能:	将帧缓冲中指定区域刷到屏幕
 * 说    明:	区域超过 1/3 屏幕时自动转为全屏更新以提升效率
****************************************************************************************************************************************/

void LCD_UpdateArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	uint32_t pixels = (uint32_t)w * h;

	/* å¤§é¢ç§¯ç´æ¥å¨å±æ´æ° */
	if (pixels > (uint32_t)(LCD.Width * LCD.Height) / 3)
	{
		LCD_Update();
		return;
	}

	LCD_SetAddress(x, y, x + w - 1, y + h - 1);

	/* åæ¢ä¸º 16 ä½æ°æ®å®½åº¦ */
	LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;
	HAL_SPI_Init(&LCD_SPI);

	LCD_DC_Data;

	/* å°åºåï¼æ·è´å° LCD_Buff ä¸æ¬¡è¿ç»­åéï¼åªéä¸æ¬¡ SPI äºå¡ */
	if (pixels <= (sizeof(LCD_Buff) / sizeof(LCD_Buff[0])))
	{
		uint32_t idx = 0;
		for (uint16_t row = 0; row < h; row++)
		{
			uint32_t offset = (y + row) * LCD.Width + x;
			memcpy(&LCD_Buff[idx], &LCD_FrameBuf[offset], w * sizeof(uint16_t));
			idx += w;
		}
		SCB_CleanDCache_by_Addr((uint32_t *)LCD_Buff, pixels * sizeof(uint16_t));
		LCD_SPI_TransmitBuffer(&LCD_SPI, LCD_Buff, pixels);
	}
	else
	{
		/* è¾å¤§åºåï¼éè¡é»å¡åé */
		for (uint16_t row = 0; row < h; row++)
		{
			uint32_t offset = (y + row) * LCD.Width + x;
			LCD_SPI_TransmitBuffer(&LCD_SPI, &LCD_FrameBuf[offset], w);
		}
	}

	/* åå 8 ä½æ°æ®å®½åº¦ */
	LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
	HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_SetAddress
 * 函数功能:	设置需要显示的坐标区域
****************************************************************************************************************************************/

void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	LCD_WriteCommand(0x2a);			// 列地址（X坐标）
	LCD_WriteData_16bit(x1 + LCD.X_Offset);
	LCD_WriteData_16bit(x2 + LCD.X_Offset);

	LCD_WriteCommand(0x2b);			// 行地址（Y坐标）
	LCD_WriteData_16bit(y1 + LCD.Y_Offset);
	LCD_WriteData_16bit(y2 + LCD.Y_Offset);

	LCD_WriteCommand(0x2c);			// 开始写入显存
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_SetDirection
 * 函数功能:	设置显示方向
****************************************************************************************************************************************/

void LCD_SetDirection(uint8_t direction)
{
	LCD.Direction = direction;

	if (direction == Direction_H)
	{
		LCD_WriteCommand(0x36);
		LCD_WriteData_8bit(0x70);
		LCD.X_Offset = 0; LCD.Y_Offset = 0;
		LCD.Width  = LCD_Height;
		LCD.Height = LCD_Width;
	}
	else if (direction == Direction_V)
	{
		LCD_WriteCommand(0x36);
		LCD_WriteData_8bit(0x00);
		LCD.X_Offset = 0; LCD.Y_Offset = 0;
		LCD.Width  = LCD_Width;
		LCD.Height = LCD_Height;
	}
	else if (direction == Direction_H_Flip)
	{
		LCD_WriteCommand(0x36);
		LCD_WriteData_8bit(0xA0);
		LCD.X_Offset = 0; LCD.Y_Offset = 0;
		LCD.Width  = LCD_Height;
		LCD.Height = LCD_Width;
	}
	else if (direction == Direction_V_Flip)
	{
		LCD_WriteCommand(0x36);
		LCD_WriteData_8bit(0xC0);
		LCD.X_Offset = 0; LCD.Y_Offset = 0;
		LCD.Width  = LCD_Width;
		LCD.Height = LCD_Height;
	}
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_Clear
 * 函数功能:	清空帧缓冲（填指定色）并刷到屏幕
****************************************************************************************************************************************/

void LCD_Clear(uint16_t color)
{
	uint32_t i;
	for (i = 0; i < (uint32_t)LCD.Width * LCD.Height; i++)
		LCD_FrameBuf[i] = color;
	LCD_Update();
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_ClearRect
 * 函数功能:	将帧缓冲中指定区域填充为指定颜色（需手动调用 LCD_UpdateArea 刷新）
****************************************************************************************************************************************/

void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
	for (uint16_t row = 0; row < height; row++)
	{
		uint32_t offset = (y + row) * LCD.Width + x;
		uint16_t *p = &LCD_FrameBuf[offset];
		for (uint16_t col = 0; col < width; col++)
			p[col] = color;
	}
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_SetPixel
 * 函数功能:	在帧缓冲中绘制一个像素点（需调用 LCD_Update 刷新）
****************************************************************************************************************************************/

void LCD_SetPixel(uint16_t x, uint16_t y, uint16_t color)
{
	LCD_FrameBuf[y * LCD.Width + x] = color;
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_PrintASCIIChar
 * 函数功能:	在帧缓冲渲染一个ASCII字符（透明背景，参考项目风格）
****************************************************************************************************************************************/

void LCD_PrintASCIIChar(uint16_t x, uint16_t y, uint8_t c, const pFONT *font, uint16_t color)
{
	if (!font || c < ' ' || c > '~') return;

	uint16_t w = font->Width;
	uint16_t h = font->Height;
	uint16_t idx = (c - 32) * font->Sizes;
	uint8_t bpc = (h + 7) / 8;       // 每列字节数（逐列式存储）

	for (uint16_t col = 0; col < w; col++)
	{
		for (uint16_t b = 0; b < bpc; b++)
		{
			uint8_t data = font->pTable[idx + col * bpc + b];
			for (uint8_t bit = 0; bit < 8; bit++)
			{
				uint16_t row = b * 8 + bit;
				if (row >= h) break;
				if ((data >> bit) & 0x01)
				{
					uint32_t fb_offset = (y + row) * LCD.Width + (x + col);
					if (fb_offset < (uint32_t)LCD.Width * LCD.Height)
						LCD_FrameBuf[fb_offset] = color;
				}
			}
		}
	}
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_PrintASCIIString
****************************************************************************************************************************************/

void LCD_PrintASCIIString(uint16_t x, uint16_t y, char *str, const pFONT *font, uint16_t color)
{
	while (*str && x < LCD.Width)
	{
		LCD_PrintASCIIChar(x, y, *str, font, color);
		x += font->Width;
		str++;
	}
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_PrintString
 * 函数功能:	显示字符串（中文+ASCII混合，使用 Font 类型，参考项目风格）
****************************************************************************************************************************************/

void LCD_PrintString(uint16_t x, uint16_t y, char *str, const pFONT *font, uint16_t color)
{
	// 简化实现：同 PrintASCIIString（中文渲染需要 Font 类型的字库支持，
	// 当前项目的中文字体 font32x32 使用参考项目的 Font 结构体，pFONT 暂不支持）
	LCD_PrintASCIIString(x, y, str, font, color);
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_ShowNumMode
****************************************************************************************************************************************/

void LCD_ShowNumMode(uint8_t mode)
{
	LCD.ShowNum_Mode = mode;
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_DisplayNumber
****************************************************************************************************************************************/

void LCD_DisplayNumber(uint16_t x, uint16_t y, int32_t number, uint8_t len, uint16_t color)
{
	char Number_Buffer[15];

	if (LCD.ShowNum_Mode == Fill_Zero)
		sprintf(Number_Buffer, "%0.*d", len, number);
	else
		sprintf(Number_Buffer, "%*d", len, number);

	LCD_PrintASCIIString(x, y, Number_Buffer, &ASCII_Font24, color);
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_DisplayDecimals
****************************************************************************************************************************************/

void LCD_DisplayDecimals(uint16_t x, uint16_t y, double num, uint8_t len, uint8_t dec, uint16_t color)
{
	char Number_Buffer[20];

	if (LCD.ShowNum_Mode == Fill_Zero)
		sprintf(Number_Buffer, "%0*.*lf", len, dec, num);
	else
		sprintf(Number_Buffer, "%*.*lf", len, dec, num);

	LCD_PrintASCIIString(x, y, Number_Buffer, &ASCII_Font24, color);
}

/****************************************************************************************************************************************
 * 绘图函数 — 全部写入帧缓冲，直接传颜色参数（参考项目风格）
****************************************************************************************************************************************/

#define ABS(X)  ((X) > 0 ? (X) : -(X))

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	int16_t deltax = ABS(x2 - x1);
	int16_t deltay = ABS(y2 - y1);
	int16_t x = x1, y = y1;
	int16_t xinc1 = (x2 >= x1) ? 1 : -1;
	int16_t yinc1 = (y2 >= y1) ? 1 : -1;
	int16_t xinc2 = xinc1, yinc2 = yinc1;
	int16_t den, num, numadd, numpixels;

	if (deltax >= deltay)
	{
		xinc1 = 0; yinc2 = 0;
		den = deltax; num = deltax / 2;
		numadd = deltay; numpixels = deltax;
	}
	else
	{
		xinc2 = 0; yinc1 = 0;
		den = deltay; num = deltay / 2;
		numadd = deltax; numpixels = deltay;
	}

	for (int16_t cur = 0; cur <= numpixels; cur++)
	{
		LCD_FrameBuf[y * LCD.Width + x] = color;
		num += numadd;
		if (num >= den)
		{
			num -= den;
			x += xinc1; y += yinc1;
		}
		x += xinc2; y += yinc2;
	}
}

void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height, uint16_t color)
{
	for (uint16_t i = 0; i < height; i++)
	{
		if ((y + i) < LCD.Height)
			LCD_FrameBuf[(y + i) * LCD.Width + x] = color;
	}
}

void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width, uint16_t color)
{
	uint32_t offset = (uint32_t)y * LCD.Width + x;
	for (uint16_t i = 0; i < width && (x + i) < LCD.Width; i++)
		LCD_FrameBuf[offset + i] = color;
}

void LCD_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
	LCD_DrawLine_H(x, y, w, color);
	LCD_DrawLine_H(x, y + h - 1, w, color);
	LCD_DrawLine_V(x, y, h, color);
	LCD_DrawLine_V(x + w - 1, y, h, color);
}

void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
	int Xadd = -r, Yadd = 0, err = 2 - 2 * r, e2;

	do {
		if ((uint32_t)(y + Yadd) < LCD.Height && (uint32_t)(x - Xadd) < LCD.Width)
			LCD_FrameBuf[(y + Yadd) * LCD.Width + (x - Xadd)] = color;
		if ((uint32_t)(y + Yadd) < LCD.Height && (uint32_t)(x + Xadd) < LCD.Width)
			LCD_FrameBuf[(y + Yadd) * LCD.Width + (x + Xadd)] = color;
		if ((uint32_t)(y - Yadd) < LCD.Height && (uint32_t)(x + Xadd) < LCD.Width)
			LCD_FrameBuf[(y - Yadd) * LCD.Width + (x + Xadd)] = color;
		if ((uint32_t)(y - Yadd) < LCD.Height && (uint32_t)(x - Xadd) < LCD.Width)
			LCD_FrameBuf[(y - Yadd) * LCD.Width + (x - Xadd)] = color;

		e2 = err;
		if (e2 <= Yadd) { err += ++Yadd * 2 + 1; if (-Xadd == Yadd && e2 <= Xadd) e2 = 0; }
		if (e2 > Xadd) err += ++Xadd * 2 + 1;
	} while (Xadd <= 0);
}

void LCD_DrawEllipse(int x, int y, int r1, int r2, uint16_t color)
{
	int Xadd = -r1, Yadd = 0, err = 2 - 2 * r1, e2;
	float K;

	if (r1 > r2)
	{
		do {
			K = r1 / (float)r2;
			int py = (int)(Yadd / K);
			if ((uint32_t)(y + py) < LCD.Height && (uint32_t)(x - Xadd) < LCD.Width) LCD_FrameBuf[(y + py) * LCD.Width + (x - Xadd)] = color;
			if ((uint32_t)(y + py) < LCD.Height && (uint32_t)(x + Xadd) < LCD.Width) LCD_FrameBuf[(y + py) * LCD.Width + (x + Xadd)] = color;
			if ((uint32_t)(y - py) < LCD.Height && (uint32_t)(x + Xadd) < LCD.Width) LCD_FrameBuf[(y - py) * LCD.Width + (x + Xadd)] = color;
			if ((uint32_t)(y - py) < LCD.Height && (uint32_t)(x - Xadd) < LCD.Width) LCD_FrameBuf[(y - py) * LCD.Width + (x - Xadd)] = color;
			e2 = err;
			if (e2 <= Yadd) { err += ++Yadd * 2 + 1; if (-Xadd == Yadd && e2 <= Xadd) e2 = 0; }
			if (e2 > Xadd) err += ++Xadd * 2 + 1;
		} while (Xadd <= 0);
	}
	else
	{
		Yadd = -r2; Xadd = 0;
		do {
			K = r2 / (float)r1;
			int px = (int)(Xadd / K);
			if ((uint32_t)(y + Yadd) < LCD.Height && (uint32_t)(x - px) < LCD.Width) LCD_FrameBuf[(y + Yadd) * LCD.Width + (x - px)] = color;
			if ((uint32_t)(y + Yadd) < LCD.Height && (uint32_t)(x + px) < LCD.Width) LCD_FrameBuf[(y + Yadd) * LCD.Width + (x + px)] = color;
			if ((uint32_t)(y - Yadd) < LCD.Height && (uint32_t)(x + px) < LCD.Width) LCD_FrameBuf[(y - Yadd) * LCD.Width + (x + px)] = color;
			if ((uint32_t)(y - Yadd) < LCD.Height && (uint32_t)(x - px) < LCD.Width) LCD_FrameBuf[(y - Yadd) * LCD.Width + (x - px)] = color;
			e2 = err;
			if (e2 <= Xadd) { err += ++Xadd * 3 + 1; if (-Yadd == Xadd && e2 <= Yadd) e2 = 0; }
			if (e2 > Yadd) err += ++Yadd * 3 + 1;
		} while (Yadd <= 0);
	}
}

/****************************************************************************************************************************************
 * 填充函数
****************************************************************************************************************************************/

void LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
	for (uint16_t row = 0; row < h; row++)
	{
		uint32_t offset = (y + row) * LCD.Width + x;
		for (uint16_t col = 0; col < w; col++)
			LCD_FrameBuf[offset + col] = color;
	}
}

void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
	int32_t D = 3 - (r << 1);
	uint32_t CurX = 0, CurY = r;

	while (CurX <= CurY)
	{
		if (CurY > 0)
		{
			uint32_t base1 = (y - CurY) * LCD.Width + x - CurX;
			uint32_t base2 = (y + CurY) * LCD.Width + x - CurX;
			for (uint32_t i = 0; i <= 2 * CurX; i++)
			{
				if ((uint32_t)(x - CurX + i) < LCD.Width)
				{
					if ((uint32_t)(y - CurY) < LCD.Height) LCD_FrameBuf[base1 + i] = color;
					if ((uint32_t)(y + CurY) < LCD.Height) LCD_FrameBuf[base2 + i] = color;
				}
			}
		}
		if (CurX > 0)
		{
			uint32_t base1 = (y - CurX) * LCD.Width + x - CurY;
			uint32_t base2 = (y + CurX) * LCD.Width + x - CurY;
			for (uint32_t i = 0; i <= 2 * CurY; i++)
			{
				if ((uint32_t)(x - CurY + i) < LCD.Width)
				{
					if ((uint32_t)(y - CurX) < LCD.Height) LCD_FrameBuf[base1 + i] = color;
					if ((uint32_t)(y + CurX) < LCD.Height) LCD_FrameBuf[base2 + i] = color;
				}
			}
		}
		if (D < 0)
			D += (CurX << 2) + 6;
		else
		{
			D += ((CurX - CurY) << 2) + 10;
			CurY--;
		}
		CurX++;
	}
	LCD_DrawCircle(x, y, r, color);
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_CopyBuffer
 * 函数功能:	将外部数据缓冲拷贝到帧缓冲并刷新到屏幕
****************************************************************************************************************************************/

void LCD_CopyBuffer(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *DataBuff)
{
	/* 拷贝到帧缓冲 */
	for (uint16_t row = 0; row < height; row++)
	{
		memcpy(&LCD_FrameBuf[(y + row) * LCD.Width + x],
			   &DataBuff[row * width],
			   width * sizeof(uint16_t));
	}

	/* 刷新到屏幕 */
	LCD_SetAddress(x, y, x + width - 1, y + height - 1);

	uint32_t pixels = (uint32_t)width * height;

	if (pixels > 32)
	{
		/* 需要先搬到一个连续缓冲再 DMA，因为 DataBuff 行宽可能不等于 LCD.Width */
		/* LCD_Buff 不够大时分多次发送 */
		uint32_t remaining = pixels;
		uint16_t *src = DataBuff;

		while (remaining > 0)
		{
			uint32_t chunk = (remaining > 1024) ? 1024 : remaining;
			memcpy(LCD_Buff, src, chunk * sizeof(uint16_t));
			LCD_SPI_TransmitBuffer_DMA(LCD_Buff, chunk);
			src += chunk;
			remaining -= chunk;
		}
	}
	else
	{
		LCD_DC_Data;
		LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;
		HAL_SPI_Init(&LCD_SPI);
		LCD_SPI_TransmitBuffer(&LCD_SPI, DataBuff, pixels);
		LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
		HAL_SPI_Init(&LCD_SPI);
	}
}

/****************************************************************************************************************************************
 * 以下为 SPI 底层传输函数（修改于 HAL 库函数，不限数据长度）
****************************************************************************************************************************************/

HAL_StatusTypeDef LCD_SPI_WaitOnFlagUntilTimeout(SPI_HandleTypeDef *hspi, uint32_t Flag, FlagStatus Status,
													uint32_t Tickstart, uint32_t Timeout)
{
	while ((__HAL_SPI_GET_FLAG(hspi, Flag) ? SET : RESET) == Status)
	{
		if ((((HAL_GetTick() - Tickstart) >= Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
			return HAL_TIMEOUT;
	}
	return HAL_OK;
}

void LCD_SPI_CloseTransfer(SPI_HandleTypeDef *hspi)
{
	uint32_t itflag = hspi->Instance->SR;

	__HAL_SPI_CLEAR_EOTFLAG(hspi);
	__HAL_SPI_CLEAR_TXTFFLAG(hspi);
	__HAL_SPI_DISABLE(hspi);
	__HAL_SPI_DISABLE_IT(hspi, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP | SPI_IT_UDR | SPI_IT_OVR | SPI_IT_FRE | SPI_IT_MODF));
	CLEAR_BIT(hspi->Instance->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);

	if (hspi->State != HAL_SPI_STATE_BUSY_RX)
	{
		if ((itflag & SPI_FLAG_UDR) != 0UL)
		{
			SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_UDR);
			__HAL_SPI_CLEAR_UDRFLAG(hspi);
		}
	}
	if (hspi->State != HAL_SPI_STATE_BUSY_TX)
	{
		if ((itflag & SPI_FLAG_OVR) != 0UL)
		{
			SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_OVR);
			__HAL_SPI_CLEAR_OVRFLAG(hspi);
		}
	}
	if ((itflag & SPI_FLAG_MODF) != 0UL)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_MODF);
		__HAL_SPI_CLEAR_MODFFLAG(hspi);
	}
	if ((itflag & SPI_FLAG_FRE) != 0UL)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FRE);
		__HAL_SPI_CLEAR_FREFLAG(hspi);
	}

	hspi->TxXferCount = 0UL;
	hspi->RxXferCount = 0UL;
}

/****************************************************************************************************************************************
 * 函 数 名:	HAL_SPI_TxCpltCallback
 * 函数功能:	SPI DMA 传输完成回调
****************************************************************************************************************************************/

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi->Instance == SPI5)
	{
		lcd_dma_done = 1;
	}
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_SPI_TransmitBuffer_DMA
 * 入口参数:	pData - 16位像素数据缓冲区，Size - 像素数量
 * 函数功能:	使用 DMA 批量传输像素数据到屏幕（含 D-Cache 刷写）
****************************************************************************************************************************************/

static HAL_StatusTypeDef LCD_SPI_TransmitBuffer_DMA(uint16_t *pData, uint32_t Size)
{
	LCD_DC_Data;

	LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;
	HAL_SPI_Init(&LCD_SPI);

	/* D-Cache 已在调用前刷写，此处再次确保 */
	SCB_CleanDCache_by_Addr((uint32_t *)pData, Size * sizeof(uint16_t));

	/*
	 * HAL_SPI_Transmit_DMA 的 Size 参数为 uint16_t，最大 65535。
	 * 超限时自动回退到阻塞传输（LCD_SPI_TransmitBuffer），避免拆包导致 CS 拉高中断 GRAM 写入。
	 */
	if (Size > 65535UL)
	{
		HAL_StatusTypeDef ret = LCD_SPI_TransmitBuffer(&LCD_SPI, pData, Size);
		LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
		HAL_SPI_Init(&LCD_SPI);
		return ret;
	}

	lcd_dma_done = 0;
	HAL_StatusTypeDef ret = HAL_SPI_Transmit_DMA(&LCD_SPI, (uint8_t *)pData, (uint16_t)Size);
	if (ret != HAL_OK)
	{
		LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
		HAL_SPI_Init(&LCD_SPI);
		return ret;
	}

	uint32_t tickstart = HAL_GetTick();
	while (!lcd_dma_done)
	{
		if (HAL_GetTick() - tickstart > 2000)
		{
			HAL_SPI_DMAStop(&LCD_SPI);
			LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
		HAL_SPI_Init(&LCD_SPI);
			return HAL_TIMEOUT;
		}
	}

	LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
	HAL_SPI_Init(&LCD_SPI);

	return HAL_OK;
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_SPI_Transmit
 * 函数功能:	专为清屏/填充修改，不限数据长度，16位重复值传输
****************************************************************************************************************************************/

HAL_StatusTypeDef LCD_SPI_Transmit(SPI_HandleTypeDef *hspi, uint16_t pData, uint32_t Size)
{
	uint32_t tickstart;
	uint32_t Timeout = 1000;
	uint32_t LCD_pData_32bit;
	uint32_t LCD_TxDataCount;
	HAL_StatusTypeDef errorcode = HAL_OK;

	assert_param(IS_SPI_DIRECTION_2LINES_OR_1LINE_2LINES_TXONLY(hspi->Init.Direction));
	__HAL_LOCK(hspi);

	tickstart = HAL_GetTick();
	if (hspi->State != HAL_SPI_STATE_READY) { errorcode = HAL_BUSY; __HAL_UNLOCK(hspi); return errorcode; }
	if (Size == 0UL) { errorcode = HAL_ERROR; __HAL_UNLOCK(hspi); return errorcode; }

	hspi->State = HAL_SPI_STATE_BUSY_TX;
	hspi->ErrorCode = HAL_SPI_ERROR_NONE;

	LCD_TxDataCount = Size;
	LCD_pData_32bit = (pData << 16) | pData;

	hspi->pRxBuffPtr = NULL;
	hspi->RxXferSize = 0UL;
	hspi->RxXferCount = 0UL;
	hspi->TxISR = NULL;
	hspi->RxISR = NULL;

	if (hspi->Init.Direction == SPI_DIRECTION_1LINE)
		SPI_1LINE_TX(hspi);

	MODIFY_REG(hspi->Instance->CR2, SPI_CR2_TSIZE, 0);
	__HAL_SPI_ENABLE(hspi);

	if (hspi->Init.Mode == SPI_MODE_MASTER)
		SET_BIT(hspi->Instance->CR1, SPI_CR1_CSTART);

	while (LCD_TxDataCount > 0UL)
	{
		if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXP))
		{
			if ((hspi->TxXferCount > 1UL) && (hspi->Init.FifoThreshold > SPI_FIFO_THRESHOLD_01DATA))
			{
				*((__IO uint32_t *)&hspi->Instance->TXDR) = LCD_pData_32bit;
				LCD_TxDataCount -= 2UL;
			}
			else
			{
				*((__IO uint16_t *)&hspi->Instance->TXDR) = pData;
				LCD_TxDataCount--;
			}
		}
		else
		{
			if ((((HAL_GetTick() - tickstart) >= Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
			{
				LCD_SPI_CloseTransfer(hspi);
				__HAL_UNLOCK(hspi);
				SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_TIMEOUT);
				hspi->State = HAL_SPI_STATE_READY;
				return HAL_ERROR;
			}
		}
	}

	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_SR_TXC, RESET, tickstart, Timeout) != HAL_OK)
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);

	SET_BIT(hspi->Instance->CR1, SPI_CR1_CSUSP);
	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_FLAG_SUSP, RESET, tickstart, Timeout) != HAL_OK)
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);

	LCD_SPI_CloseTransfer(hspi);
	SET_BIT(hspi->Instance->IFCR, SPI_IFCR_SUSPC);

	__HAL_UNLOCK(hspi);
	hspi->State = HAL_SPI_STATE_READY;

	return (hspi->ErrorCode != HAL_SPI_ERROR_NONE) ? HAL_ERROR : errorcode;
}

/****************************************************************************************************************************************
 * 函 数 名:	LCD_SPI_TransmitBuffer
 * 函数功能:	专为批量写入数据修改，不限长度传输16位数据
****************************************************************************************************************************************/

HAL_StatusTypeDef LCD_SPI_TransmitBuffer(SPI_HandleTypeDef *hspi, uint16_t *pData, uint32_t Size)
{
	uint32_t tickstart;
	uint32_t Timeout = 1000;
	uint32_t LCD_TxDataCount;
	HAL_StatusTypeDef errorcode = HAL_OK;

	assert_param(IS_SPI_DIRECTION_2LINES_OR_1LINE_2LINES_TXONLY(hspi->Init.Direction));
	__HAL_LOCK(hspi);

	tickstart = HAL_GetTick();
	if (hspi->State != HAL_SPI_STATE_READY) { errorcode = HAL_BUSY; __HAL_UNLOCK(hspi); return errorcode; }
	if (Size == 0UL) { errorcode = HAL_ERROR; __HAL_UNLOCK(hspi); return errorcode; }

	hspi->State = HAL_SPI_STATE_BUSY_TX;
	hspi->ErrorCode = HAL_SPI_ERROR_NONE;
	LCD_TxDataCount = Size;

	hspi->pRxBuffPtr = NULL;
	hspi->RxXferSize = 0UL;
	hspi->RxXferCount = 0UL;
	hspi->TxISR = NULL;
	hspi->RxISR = NULL;

	if (hspi->Init.Direction == SPI_DIRECTION_1LINE)
		SPI_1LINE_TX(hspi);

	MODIFY_REG(hspi->Instance->CR2, SPI_CR2_TSIZE, 0);
	__HAL_SPI_ENABLE(hspi);

	if (hspi->Init.Mode == SPI_MODE_MASTER)
		SET_BIT(hspi->Instance->CR1, SPI_CR1_CSTART);

	while (LCD_TxDataCount > 0UL)
	{
		if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXP))
		{
			if ((LCD_TxDataCount > 1UL) && (hspi->Init.FifoThreshold > SPI_FIFO_THRESHOLD_01DATA))
			{
				*((__IO uint32_t *)&hspi->Instance->TXDR) = *((uint32_t *)pData);
				pData += 2;
				LCD_TxDataCount -= 2;
			}
			else
			{
				*((__IO uint16_t *)&hspi->Instance->TXDR) = *pData;
				pData++;
				LCD_TxDataCount--;
			}
		}
		else
		{
			if ((((HAL_GetTick() - tickstart) >= Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
			{
				LCD_SPI_CloseTransfer(hspi);
				__HAL_UNLOCK(hspi);
				SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_TIMEOUT);
				hspi->State = HAL_SPI_STATE_READY;
				return HAL_ERROR;
			}
		}
	}

	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_SR_TXC, RESET, tickstart, Timeout) != HAL_OK)
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);

	SET_BIT(hspi->Instance->CR1, SPI_CR1_CSUSP);
	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_FLAG_SUSP, RESET, tickstart, Timeout) != HAL_OK)
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);

	LCD_SPI_CloseTransfer(hspi);
	SET_BIT(hspi->Instance->IFCR, SPI_IFCR_SUSPC);

	__HAL_UNLOCK(hspi);
	hspi->State = HAL_SPI_STATE_READY;

	return (hspi->ErrorCode != HAL_SPI_ERROR_NONE) ? HAL_ERROR : errorcode;
}
