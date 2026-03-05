#include "oled_driver.h"
#include "oled_font.h"
#include "i2c.h"
#include <string.h>

uint8_t OLED_Buffer[OLED_BUFFER_SIZE];// OLED 显存缓冲区
volatile uint8_t oled_ready = 1;

static FIL fontFile;
static uint8_t font_init_done = 0;

static const uint8_t init_cmds[] = {
    0xAE,
    0x20, 0x00,
    0xB0,
    0xC8,
    0x00, 0x10,
    0x40,
    0x81, 0xDF,
    0xA1,
    0xA6,
    0xA8, 0x3F,
    0xA4,
    0xD3, 0x00,
    0xD5, 0xF0,
    0xD9, 0x22,
    0xDA, 0x12,
    0xDB, 0x20,
    0x8D, 0x14,
    0xAF
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



void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {
    int16_t dx = (x1 < x2) ? (x2 - x1) : (x1 - x2);
    int16_t dy = (y1 < y2) ? (y2 - y1) : (y1 - y2);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        OLED_DrawPoint(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

// 补全垂直线，频谱绘制必备
void OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t h, uint8_t color) {
    for (uint8_t i = y; i < y + h; i++) {
        OLED_DrawPoint(x, i, color);
    }
}


// 调用 FatFs 内部的转码函数
// dir = 1 代表从 OEM(GBK) 转 Unicode
extern WCHAR ff_convert (WCHAR src, UINT dir);

/**
 * 针对 GBK 编码优化的中文显示函数
 * 逻辑：识别 GBK 双字节 -> 转为 Unicode -> 查 SD 卡字库
 */
void OLED_ShowSDString(uint8_t x, uint8_t y, const char* str) {
    uint8_t* p = (uint8_t*)str;
    uint32_t unicode_code;

    while (*p) {
        if (*p < 0x80) {
            //  1. 处理标准 ASCII (英文/数字)
            OLED_ShowChar(x, y, (char)*p, 1);
            x += 8;
            p++;
        }
        else {
            //  2. 处理 GBK 中文
            // 将两个字节拼成一个 16 位的 GBK 码
            // 注意：GBK 是大端序，第一个字节在高位
            uint16_t gbk_val = (*p << 8) | *(p + 1);

            // 使用 FatFs 内部表进行转码
            unicode_code = ff_convert(gbk_val, 1);

            // 如果转码成功（不为 0），则去 SD 卡读字库
            if (unicode_code != 0) {
                OLED_DrawCJKChar(x, y, unicode_code);
            } else {
                // 如果转码失败，画个空心框占位
                OLED_DrawRect(x, y, 16, 16, 1);
            }

            x += 16;
            p += 2; // 跳过处理过的两个字节
        }

        // 边界检查：防止一行显示太多导致溢出
        if (x > 120) break;
    }
}


/**
 * 显示任意大小的图片
 * x, y: 图片左上角坐标
 * w, h: 图片的宽度和高度
 * data: 图片的点阵数据数组
 */
void OLED_DrawImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *data) {
    // 计算图片每行占用的字节数 (向上取整)
    uint16_t byteWidth = (w + 7) / 8;

    for (uint8_t j = 0; j < h; j++) { // 遍历行
        for (uint8_t i = 0; i < w; i++) { // 遍历列
            // 核心逻辑：
            // data[j * byteWidth + (i / 8)] 找到当前像素所在的字节
            // (0x80 >> (i % 8)) 找到该字节内对应的位 (MSB First 高位在前)
            if (data[j * byteWidth + (i / 8)] & (0x80 >> (i % 8))) {
                OLED_DrawPoint(x + i, y + j, 1);
            }
        }
    }
}