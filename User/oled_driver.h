#ifndef __OLED_DRIVER_H
#define __OLED_DRIVER_H

#include "ff.h"
#include "main.h"

// 屏幕参数
#define OLED_WIDTH   128
#define OLED_HEIGHT  32
#define OLED_BUFFER_SIZE 512
#define OLED_ADDR    0x78

// 字库参数
#define FONT_SD_BYTES  32

// 1. 基础系统接口
void OLED_Init(void);
void OLED_Update(void);
void OLED_Clear(void);

// 2. 基础绘图接口
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t percent);

// 3. 字体接口 (Flash 里的英文)
void OLED_ShowChar(int16_t x, int16_t y, char chr, uint8_t color);
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t color);

// 4. 字库接口 (SD 卡里的中日文)
FRESULT OLED_FontInit(const char* path);
void OLED_DrawCJKChar(int16_t x, int16_t y, uint32_t unicode);

// 外部引用的变量
extern uint8_t OLED_Buffer[OLED_BUFFER_SIZE];
extern volatile uint8_t oled_ready;

#endif