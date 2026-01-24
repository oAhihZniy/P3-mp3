#include "oled_driver.h"
#include "oled_font.h"
#include "i2c.h"
#include <string.h>

uint8_t OLED_Buffer[OLED_BUFFER_SIZE];
volatile uint8_t oled_ready = 1;

static FIL fontFile;           // 唯一的字库句柄
static uint8_t font_init_done = 0;

// SSD1306 初始化序列
static const uint8_t init_cmds[] = {
    0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40, 0x81, 0x7F,
    0xA1, 0xA6, 0xA8, 0x1F, 0xAD, 0x8B, 0xD3, 0x00, 0xD5, 0xF0,
    0xD9, 0x22, 0xDA, 0x02, 0xDB, 0x40, 0xAF
};

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

void OLED_Update(void) {
    if (oled_ready) {
        oled_ready = 0;
        HAL_I2C_Mem_Write_DMA(&hi2c1, OLED_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, OLED_Buffer, OLED_BUFFER_SIZE);
    }
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C1) oled_ready = 1;
}

void OLED_Clear(void) { memset(OLED_Buffer, 0, OLED_BUFFER_SIZE); }

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color) {
    if(x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    if(color) OLED_Buffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
    else OLED_Buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
}

void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t i = x; i < x + w; i++) { OLED_DrawPoint(i, y, color); OLED_DrawPoint(i, y + h - 1, color); }
    for (uint8_t i = y; i < y + h; i++) { OLED_DrawPoint(x, i, color); OLED_DrawPoint(x + w - 1, i, color); }
}

void OLED_DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t i = x; i < x + w; i++)
        for (uint8_t j = y; j < y + h; j++) OLED_DrawPoint(i, j, color);
}

void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t percent) {
    if (percent > 100) percent = 100;
    OLED_DrawRect(x, y, w, h, 1);
    uint8_t fill_w = (w - 4) * percent / 100;
    if (fill_w > 0) OLED_DrawFilledRect(x + 2, y + 2, fill_w, h - 4, 1);
}

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

void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t color) {
    while (*str) { OLED_ShowChar(x, y, *str, color); x += 8; str++; }
}

FRESULT OLED_FontInit(const char* path) {
    FRESULT res = f_open(&fontFile, path, FA_READ);
    if (res == FR_OK) font_init_done = 1;
    return res;
}

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

void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {
    // 这里可以使用简单的 Bresenham 算法，或者暂时留空防止报错
    for (uint8_t i = x1; i <= x2; i++) OLED_DrawPoint(i, y1, color); // 简单的水平线占位
}