#include "oled_app.h"

#include <string.h>
#include "oled_driver.h"


/**
 * 绘制平滑滚动的歌名
 * y: 纵坐标 (建议 12-16)
 * str: 歌名字符串
 * tick: 系统时间戳或计数值，用于控制位移
 */
void UI_DrawScrollingTitle(uint8_t y, const char* str, uint32_t tick) {
    uint16_t len = strlen(str);
    uint16_t text_width = len * 8; // ASCII 8x16 字体宽度
    int16_t x_pos;

    if (text_width <= 128) {
        // 如果歌名短，居中显示
        OLED_ShowString((128 - text_width) / 2, y, (char*)str, 1);
    } else {
        // 滚动逻辑：每 30ms 移动 1 像素
        // 循环长度 = 文字宽度 + 40 像素留白
        uint16_t loop_width = text_width + 40;
        uint16_t offset = (tick / 30) % loop_width;

        x_pos = -offset;

        // 绘制第一遍
        UI_OLED_ShowScrollString(x_pos, y, (char*)str);
        // 如果第一遍快走完了，把第二遍接在后面，实现无缝循环
        if (x_pos < (128 - text_width)) {
            UI_OLED_ShowScrollString(x_pos + loop_width, y, (char*)str);
        }
    }
}

// 辅助函数：支持负数坐标的字符串显示（裁剪逻辑）
static void UI_OLED_ShowScrollString(int16_t x, uint8_t y, char* str) {
    while (*str) {
        if (x > -8 && x < 128) { // 只画在屏幕范围内的字符
            OLED_ShowChar(x, y, *str, 1);
        }
        x += 8;
        str++;
    }
}