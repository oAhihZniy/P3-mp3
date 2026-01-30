#ifndef __OLED_DRIVER_H
#define __OLED_DRIVER_H

#include "ff.h"
#include "main.h"

// 屏幕参数
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_BUFFER_SIZE 1024
#define OLED_ADDR    0x78

// 字库参数
#define FONT_SD_BYTES  32

// 1. 基础系统接口
void OLED_Init(void);
void OLED_Update(void);
void OLED_Clear(void);

// 2. 基础绘图接口
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);// 画点
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);// 画线
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);// 画矩形
void OLED_DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);// 画实心矩形
void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t percent);// 画进度条
void OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t h, uint8_t color);
// 3. 字体接口 (Flash 里的英文)
void OLED_ShowChar(int16_t x, int16_t y, char chr, uint8_t color);// 显示单个字符
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t color);// 显示字符串

// 4. 字库接口 (SD 卡里的中日文)
FRESULT OLED_FontInit(const char* path);// 初始化字库
void OLED_DrawCJKChar(int16_t x, int16_t y, uint32_t unicode);// 显示中日文字
void OLED_ShowSDString(uint8_t x, uint8_t y, const char* str);


// 外部引用的变量
extern uint8_t OLED_Buffer[OLED_BUFFER_SIZE];
extern volatile uint8_t oled_ready;

#endif