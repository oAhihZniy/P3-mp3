#include "oled_driver.h"

#include <string.h>

#include "i2c.h"
#include "oled_font.h"

uint8_t OLED_Buffer[OLED_BUFFER_SIZE];
volatile uint8_t oled_ready = 1; // 1代表空闲，0代表正在DMA传输

// SSD1306 初始化命令序列
static const uint8_t init_cmds[] = {
    0xAE,       // 关闭显示
    0x20, 0x00, // 设置寻址模式：00-水平模式 (DMA 专用)
    0xB0,       // 设置起始页地址
    0xC8,       // 翻转 COM 扫描方向
    0x00,       // 设置低列地址
    0x10,       // 设置高列地址
    0x40,       // 设置起始行地址
    0x81, 0x7F, // 对比度设置
    0xA1,       // 设置左右方向 (根据你的 PCB 焊接方向调整 A0/A1)
    0xA6,       // 正常显示 (不反相)
    0xA8, 0x1F, // 1/32 屏 (32行)
    0xAD, 0x8B, // DC-DC 开关
    0xD3, 0x00, // 设置显示偏移
    0xD5, 0xF0, // 设置分频因子
    0xD9, 0x22, // 设置预充电周期
    0xDA, 0x02, // 设置 COM 引脚硬件配置
    0xDB, 0x40, // 设置 VCOMH
    0xAF        // 开启显示
};

// 发送单条命令 (阻塞模式，仅初始化用)
static void OLED_WriteCmd(uint8_t cmd) {
    HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 10);
}

void OLED_Init(void) {
    // 硬件复位 (之前分配的 PA8)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);

    // 发送初始化命令
    for(uint8_t i=0; i<sizeof(init_cmds); i++) {
        OLED_WriteCmd(init_cmds[i]);
    }
    OLED_Clear();
    OLED_Update();
}

// 刷新屏幕：把 512 字节显存通过 DMA 发送
void OLED_Update(void) {
    if (oled_ready) {
        oled_ready = 0;
        // 0x40 代表接下来发送的是数据（显存）
        HAL_I2C_Mem_Write_DMA(&hi2c1, OLED_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, OLED_Buffer, OLED_BUFFER_SIZE);
    }
}

// DMA 传输完成回调
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C1) {
        oled_ready = 1; // 释放标志位
    }
}

void OLED_Clear(void) {
    for(uint16_t i=0; i<OLED_BUFFER_SIZE; i++) OLED_Buffer[i] = 0;
}

/**
 * 画点函数 (x: 0-127, y: 0-31, color: 1点亮, 0熄灭)
 */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color) {
    if(x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    
    if(color)
        OLED_Buffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
    else
        OLED_Buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
}

/**
 * 画水平线 (用于进度条边框)
 */
void OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t len, uint8_t color) {
    for (uint8_t i = x; i < x + len; i++) OLED_DrawPoint(i, y, color);
}

/**
 * 画垂直线
 */
void OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t len, uint8_t color) {
    for (uint8_t i = y; i < y + len; i++) OLED_DrawPoint(x, i, color);
}

/**
 * 画矩形 (空心)
 */
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    OLED_DrawHLine(x, y, w, color);             // 上
    OLED_DrawHLine(x, y + h - 1, w, color);     // 下
    OLED_DrawVLine(x, y, h, color);             // 左
    OLED_DrawVLine(x + w - 1, y, h, color);     // 右
}

/**
 * 画实心矩形 (进度条填充)
 */
void OLED_DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t i = x; i < x + w; i++) {
        for (uint8_t j = y; j < y + h; j++) {
            OLED_DrawPoint(i, j, color);
        }
    }
}



/**
 * 显示单个 ASCII 字符 (8x16)
 * x, y: 起始坐标
 * chr: 字符内容
 */
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t color) {
    uint8_t c = chr - ' '; // 计算偏移
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t temp = ASCII_8x16[c * 16 + i];
        for (uint8_t j = 0; j < 8; j++) {
            if (temp & (0x80 >> j)) {
                // 根据 SSD1306 寻址模式，这里可能需要根据你的 DrawPoint 逻辑调整
                OLED_DrawPoint(x + j, y + i, color);
            }
        }
    }
}

/**
 * 显示字符串
 */
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t color) {
    while (*str) {
        OLED_ShowChar(x, y, *str, color);
        x += 8; // 8x16 字体，x 轴平移 8 像素
        if (x > 120) break; // 超出屏幕
        str++;
    }
}


/**
 * 绘制进度条
 * x, y: 位置
 * w, h: 总宽高
 * percent: 0-100
 */
void OLED_DrawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t percent) {
    if (percent > 100) percent = 100;

    // 1. 画外框
    OLED_DrawRect(x, y, w, h, 1);

    // 2. 计算填充宽度 (缩进 2 像素看起来更精致)
    uint8_t fill_w = (w - 4) * percent / 100;

    // 3. 画内部实心条
    if (fill_w > 0) {
        OLED_DrawFilledRect(x + 2, y + 2, fill_w, h - 4, 1);
    }
}


/* main.c 循环任务 后面在编辑这个 */
void UI_Update_Task(void) {
    OLED_Clear();

    // 1. 显示状态
    OLED_ShowString(0, 0, "PLAYING", 1);

    // 2. 显示音量 (假设音量是 50)
    OLED_ShowString(80, 0, "V:50", 1);

    // 3. 显示歌名 (暂时先用英文)
    OLED_ShowString(0, 16, "Burn My Dread", 1);

    // 4. 显示播放进度 (假设当前播放到 35%)
    OLED_DrawProgressBar(0, 28, 128, 4, 35);

    OLED_Update(); // 提交 DMA 刷新
}

/**
 * 显示滚动字符串
 * x, y: 显示窗口的起始位置
 * width: 窗口的宽度 (比如歌名区宽 128)
 * str: 字符串指针
 * offset: 当前滚动的偏移量 (单位：像素)
 */
void OLED_ShowScrollString(uint8_t x, uint8_t y, uint8_t width, char *str, uint16_t offset) {
    uint16_t str_len = strlen(str);
    uint16_t total_pixel_width = str_len * 8; // 每个字符宽 8 像素

    // 如果字符串短于窗口，直接显示，不滚动
    if (total_pixel_width <= width) {
        OLED_ShowString(x, y, str, 1);
        return;
    }

    // 循环滚动逻辑
    uint16_t current_offset = offset % (total_pixel_width + 40); // 40 是留白，防止首尾相连太挤

    for (uint16_t i = 0; i < str_len; i++) {
        // 计算每个字符相对于窗口左侧的位置
        int16_t char_x = x + (i * 8) - current_offset;

        // 只绘制在窗口范围内的部分字符
        if (char_x > -8 && char_x < x + width) {
            OLED_ShowChar(char_x, y, str[i], 1);
        }
    }
}