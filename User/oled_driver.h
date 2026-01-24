#ifndef __OLED_DRIVER_H
#define __OLED_DRIVER_H

#include "ff.h"
#include "main.h"

// 128x32 屏幕参数
#define OLED_WIDTH   128
#define OLED_HEIGHT  32
#define OLED_BUFFER_SIZE (OLED_WIDTH * OLED_HEIGHT / 8) // 512 字节

// 字体大小定义
#define FONT_SD_SIZE   16
#define FONT_SD_BYTES  32  // (16 * 16 / 8)

// SSD1306 I2C 地址 (通常是 0x78)
#define OLED_ADDR    0x78

// 基础接口
void OLED_Init(void);
void OLED_Update(void); // 将显存丢给 DMA 发送
void OLED_Clear(void);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);

// 显存对象（供外部绘图函数使用）
extern uint8_t OLED_Buffer[OLED_BUFFER_SIZE];
extern volatile uint8_t oled_ready; // 标志位：DMA 是否发送完成

void OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t len, uint8_t color);
void OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t len, uint8_t color);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t color);
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t color);
void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t percent);
void UI_Update_Task(void);
void OLED_ShowScrollString(uint8_t x, uint8_t y, uint8_t width, char *str, uint16_t offset);


// 外部调用接口
void OLED_ShowSDChar(uint8_t x, uint8_t y, uint32_t unicode);
FRESULT OLED_FontInit(const char* path); // 初始化字库文件
void OLED_ShowSDString(uint8_t x, uint8_t y, const char* utf8_str);

#endif