#include "oled_app.h"
#include "oled_driver.h"
#include "audio_driver.h"
#include "playlist.h"
#include <string.h>
#include <stdio.h>

#include "i2s.h"

// 内部工具：UTF-8 解码
static uint8_t Get_Unicode_From_UTF8(const uint8_t* p, uint32_t* out_unicode) {
    if ((p[0] & 0x80) == 0) { *out_unicode = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0) { *out_unicode = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); return 2; }
    if ((p[0] & 0xF0) == 0xE0) { *out_unicode = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3; }
    return 1;
}

void UI_DrawMixedScrollTitle(uint8_t y, const char* str, uint32_t tick) {
    uint32_t unicode;
    uint8_t bytes;
    uint16_t total_w = 0;
    const uint8_t* p = (const uint8_t*)str;

    while (*p) {
        bytes = Get_Unicode_From_UTF8(p, &unicode);
        total_w += (bytes == 1) ? 8 : 16;
        p += bytes;
    }

    if (total_w <= 128) {
        int16_t start_x = (128 - total_w) / 2;
        p = (const uint8_t*)str;
        while (*p) {
            bytes = Get_Unicode_From_UTF8(p, &unicode);
            if (bytes == 1) OLED_ShowChar(start_x, y, (char)unicode, 1);
            else OLED_DrawCJKChar(start_x, y, unicode);
            start_x += (bytes == 1) ? 8 : 16;
            p += bytes;
        }
    } else {
        uint16_t loop_w = total_w + 40;
        int16_t scroll_offset = (tick / 30) % loop_w;
        int16_t draw_x = -scroll_offset;

        for (int loop = 0; loop < 2; loop++) {
            p = (const uint8_t*)str;
            int16_t temp_x = draw_x + (loop * loop_w);
            while (*p) {
                bytes = Get_Unicode_From_UTF8(p, &unicode);
                if (temp_x > -16 && temp_x < 128) {
                    if (bytes == 1) OLED_ShowChar(temp_x, y, (char)unicode, 1);
                    else OLED_DrawCJKChar(temp_x, y, unicode);
                }
                temp_x += (bytes == 1) ? 8 : 16;
                p += bytes;
                if (temp_x >= 128 && loop == 1) break;
            }
        }
    }
}

/**
 * (我是牢理) P3 风格动态波形跳动
 * x, y: 位置, w, h: 宽度和最大高度
 */
void UI_DrawDynamicSpectrum(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    // 获取 DMA 实时播放位置 (之前我们算过)
    uint32_t dma_pos = (AUDIO_BUFFER_COUNT) - __HAL_DMA_GET_COUNTER(hi2s2.hdmatx);

    for (uint8_t i = 0; i < w; i += 4) {
        // 采样 PCM 数据并取绝对值
        int16_t sample = (int16_t)AudioBuffer[(dma_pos + i * 16) % AUDIO_BUFFER_COUNT];
        if (sample < 0) sample = -sample;

        // 映射高度 (32768是16位最大振幅)
        uint8_t bar_h = (sample * h) / 16384; // 稍微放大一点灵敏度
        if (bar_h > h) bar_h = h;

        // 画出像 P3 菜单那样的细柱状条
        OLED_DrawVLine(x + i, y + (h - bar_h), bar_h, 1);
        OLED_DrawVLine(x + i + 1, y + (h - bar_h), bar_h, 1);
    }
}

/**
 * (我是牢理) 画出 P3 标志性的斜向几何装饰
 */
void UI_DrawP3Aesthetic(void) {
    // 顶部右侧的斜切角
    OLED_DrawLine(100, 0, 127, 10, 1);
    OLED_DrawLine(105, 0, 127, 8, 1);

    // 左侧的垂直条纹装饰
    OLED_DrawFilledRect(0, 15, 2, 34, 1);

    // 进度条上方的斜虚线
    for(int i=0; i<128; i+=6) {
        OLED_DrawPoint(i, 40, 1);
    }
}


void UI_Refresh_Task(void) {
    char buf[32];
    uint32_t now = HAL_GetTick();
    uint32_t sec = Audio_GetElapsedSec();
    extern uint32_t current_bw; // 引用测速变量

    OLED_Clear();

    // 1. 绘制 P3 几何背景装饰
    UI_DrawP3Aesthetic();

    // 2. 状态栏 (左上角)
    // 01/15  V:40
    snprintf(buf, sizeof(buf), "%02d/%02d  V%02d",
             g_playlist.current_index + 1, g_playlist.total_count, Audio_GetVolume());
    OLED_ShowString(4, 0, buf, 1);

    // 3. 核心区域：动态频谱 + 滚动歌名
    // 左边跳动波形，右边滚歌名
    UI_DrawDynamicSpectrum(4, 20, 32, 20);
    OLED_Update();
    UI_DrawMixedScrollTitle(16, g_playlist.current_filename, now);

    // 4. 底部进度条 (做成 P3 风格：带边框和细线)
    // 假设歌长 300s
    uint8_t progress = (sec % 300) * 100 / 300;
    OLED_DrawProgressBar(0, 44, 128, 4, progress);

    // 5. 最底部显示时间
    snprintf(buf, sizeof(buf), "TIME: %02lu:%02lu", sec / 60, sec % 60);
    OLED_ShowString(35, 48, buf, 1);

    OLED_Update();
}




