#include "oled_driver.h"
#include "oled_font.h"
#include "i2c.h"
#include <string.h>

uint8_t OLED_Buffer[OLED_BUFFER_SIZE];// OLED 显存缓冲区
volatile uint8_t oled_ready = 1;

static FIL fontFile;           // 唯一的字库句柄
static uint8_t font_init_done = 0;

// SSD1306 初始化序列
// static const uint8_t init_cmds[] = {
//     0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40, 0x81, 0x7F,
//     0xA1, 0xA6, 0xA8, 0x1F, 0xAD, 0x8B, 0xD3, 0x00, 0xD5, 0xF0,
//     0xD9, 0x22, 0xDA, 0x02, 0xDB, 0x40, 0xAF
// };

// SSD1306 初始化序列
// static const uint8_t init_cmds[] = {
//     0xAE, 0x20, 0x10, 0xB0, 0xC8, 0x00, 0x10, 0x40, 0x81, 0xDF,
//     0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3, 0x00, 0xD5, 0xF0,
//     0xD9, 0x22, 0xDA, 0x12, 0xDB, 0x20, 0x8D, 0x14, 0xAF
// };

static const uint8_t init_cmds[] = {
    0xAE,       // 关闭显示
    0x20, 0x00, // 核心：必须是 0x00 (水平寻址) 才能适配你的全屏 DMA 刷新
    0xB0,       // 起始页
    0xC8,       // 翻转 COM
    0x00, 0x10, // 列地址设置
    0x40,       // 起始行
    0x81, 0xDF, // 对比度
    0xA1,       // 左右反转
    0xA6,       // 正常显示
    0xA8, 0x3F, // 核心：64行 (0.96寸专用)
    0xA4,       // 全屏显示
    0xD3, 0x00, // 偏移
    0xD5, 0xF0, // 频率
    0xD9, 0x22, // 充电周期
    0xDA, 0x12, // 核心：Alternative COM (0.96寸专用)
    0xDB, 0x20, // VCOMH
    0x8D, 0x14, // 开启电荷泵
    0xAF        // 开启显示
};

// 发送命令
static void OLED_WriteCmd(uint8_t cmd) {
    HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 10);
}

void OLED_Init(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    for(uint8_t i=0; i<sizeof(init_cmds); i++) OLED_WriteCmd(init_cmds[i]);
    OLED_Clear();
    OLED_Update();
}

// 使用 DMA 异步更新屏幕
void OLED_Update(void) {
    if (oled_ready) {
        oled_ready = 0;
        if (HAL_I2C_Mem_Write_DMA(&hi2c1, OLED_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, OLED_Buffer, OLED_BUFFER_SIZE) != HAL_OK) {
            oled_ready = 1; // 如果启动失败，立即重置标志位
        }
    }
}

// I2C DMA 传输完成回调
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C1) {
        oled_ready = 1;
    }
}

// 清屏
void OLED_Clear(void) { memset(OLED_Buffer, 0, OLED_BUFFER_SIZE); }

// 画点
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color) {
    if(x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    if(color) OLED_Buffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
    else OLED_Buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
}

// 画矩形
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t i = x; i < x + w; i++) { OLED_DrawPoint(i, y, color); OLED_DrawPoint(i, y + h - 1, color); }
    for (uint8_t i = y; i < y + h; i++) { OLED_DrawPoint(x, i, color); OLED_DrawPoint(x + w - 1, i, color); }
}

// 画实心矩形
void OLED_DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t i = x; i < x + w; i++)
        for (uint8_t j = y; j < y + h; j++) OLED_DrawPoint(i, j, color);
}

// 画进度条
void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t percent) {
    if (percent > 100) percent = 100;
    OLED_DrawRect(x, y, w, h, 1);
    uint8_t fill_w = (w - 4) * percent / 100;
    if (fill_w > 0) OLED_DrawFilledRect(x + 2, y + 2, fill_w, h - 4, 1);
}

// 显示单个字符 (8x16)
void OLED_ShowChar(int16_t x, int16_t y, char chr, uint8_t color) {
    if (x <= -8 || x >= 128) return;
    uint8_t c = chr - ' ';
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t temp = ASCII_8x16[c * 16 + i];
        for (uint8_t j = 0; j < 8; j++) {
            if (temp & (0x80 >> j)) OLED_DrawPoint(x + j, y + i, color);
        }
    }
}

// 显示字符串
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t color) {
    while (*str) { OLED_ShowChar(x, y, *str, color); x += 8; str++; }
}

// 初始化字库
FRESULT OLED_FontInit(const char* path) {
    FRESULT res = f_open(&fontFile, path, FA_READ);
    if (res == FR_OK) font_init_done = 1;
    return res;
}

// 显示中日文字 (16x16)
void OLED_DrawCJKChar(int16_t x, int16_t y, uint32_t unicode) {
    if (!font_init_done || x <= -16 || x >= 128) return;
    uint8_t glyph[32];
    UINT br;
    f_lseek(&fontFile, unicode * 32);
    if(f_read(&fontFile, glyph, 32, &br) == FR_OK && br == 32) {
        for (uint8_t i = 0; i < 16; i++) {
            for (uint8_t j = 0; j < 8; j++) {
                if (glyph[i * 2] & (0x80 >> j)) OLED_DrawPoint(x + j, y + i, 1);
                if (glyph[i * 2 + 1] & (0x80 >> j)) OLED_DrawPoint(x + j + 8, y + i, 1);
            }
        }
    }
}

// 画线 (简单实现，仅支持水平线占位)
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {

    for (uint8_t i = x1; i <= x2; i++) OLED_DrawPoint(i, y1, color); // 简单的水平线占位
}