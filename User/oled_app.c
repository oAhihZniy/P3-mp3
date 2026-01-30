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
/**
 * (我是牢理) 混合 UTF-8 滚动标题显示函数
 * y: 垂直位置
 * str: UTF-8 字符串
 * tick: 当前时间戳 (毫秒)
 */
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
 * (我是牢理) 增强版：中英日混合滚动窗口
 * x, y: 滚动区域左上角的起始位置
 * window_w: 滚动区域的宽度 (比如你想在右边显示歌名)
 * str: 歌名 (GBK编码)
 * tick: 动画计数
 */
extern WCHAR ff_convert (WCHAR src, UINT dir); // 依然需要这个转码神器

void UI_DrawMixedScroll_Window(uint8_t x, uint8_t y, uint8_t window_w, const char* str, uint32_t tick) {
    uint32_t unicode;
    uint16_t total_w = 0;
    uint8_t* p = (uint8_t*)str;

    // 1. 计算字符串在 GBK 模式下的总像素宽度
    while (*p) {
        if (*p < 0x80) { total_w += 8; p++; }
        else { total_w += 16; p += 2; }
    }

    // 如果总长还没窗口宽，直接在 x, y 位置静态显示
    if (total_w <= window_w) {
        OLED_ShowSDString(x, y, str);
        return;
    }

    // 2. 计算滚动循环
    uint16_t loop_w = total_w + 40; // 留白 40 像素
    int16_t scroll_offset = (tick / 30) % loop_w;
    int16_t draw_start_x = -scroll_offset;

    // 3. 渲染循环 (画两遍实现无缝衔接)
    for (int loop = 0; loop < 2; loop++) {
        int16_t temp_x = draw_start_x + (loop * loop_w);
        uint8_t* p_draw = (uint8_t*)str;

        while (*p_draw) {
            uint16_t char_w = (*p_draw < 0x80) ? 8 : 16;

            // 计算字符当前的屏幕绝对坐标
            int16_t screen_x = x + temp_x;

            // (我是牢理) 关键：只有在窗口范围 [x, x + window_w] 内才画点
            if (screen_x > (int16_t)x - (int16_t)char_w && screen_x < (int16_t)x + (int16_t)window_w) {
                if (*p_draw < 0x80) {
                    OLED_ShowChar(screen_x, y, (char)*p_draw, 1);
                    p_draw++;
                } else {
                    uint16_t gbk = (*p_draw << 8) | *(p_draw + 1);
                    OLED_DrawCJKChar(screen_x, y, ff_convert(gbk, 1));
                    p_draw += 2;
                }
            } else {
                p_draw += (*p_draw < 0x80) ? 1 : 2;
            }
            temp_x += char_w;

            // 如果已经超出窗口右侧，这遍循环就不用往下读了
            if (temp_x > window_w && loop == 1) break;
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
        uint8_t bar_h = (sample * h) / 16384 ; // 稍微放大一点灵敏度
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


void UI_Refresh_Task1(void) {
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


/**
 * (我是牢理) 绘制播放/暂停状态图标
 */
void UI_DrawStatusIcon(uint8_t x, uint8_t y, AudioStatus_t status) {
    if (status == AUDIO_PLAYING) {
        // 画一个 ">" 播放图标
        OLED_ShowString(x, y, ">", 1);
    } else {
        // 画一个 "||" 暂停图标
        OLED_ShowString(x, y, "||", 1);
    }
}

void UI_Refresh_Task(void) {
    char info_str[32];
    uint32_t elapsed = Audio_GetElapsedSec();
    uint32_t now = HAL_GetTick();

    // 1. 带宽统计 (保持原样)
    static uint32_t last_stat_tick = 0;
    static uint32_t current_bw = 0;
    if (now - last_stat_tick >= 1000) {
        extern uint32_t real_time_bytes;
        current_bw = real_time_bytes / 1024;
        real_time_bytes = 0;
        last_stat_tick = now;
    }

    OLED_Clear();

    // --- A. 顶部区域 ---
    // 歌曲序号 01/15 和 测速
    snprintf(info_str, sizeof(info_str), "%02d/%02d  V%02d",
                g_playlist.current_index + 1, g_playlist.total_count, Audio_GetVolume());
    OLED_ShowString(4, 0, info_str, 1);
    // OLED_DrawVLine(0, 15, 128, 1);

    // --- B. 中部区域 (灵魂：频谱 + 滚动) ---
    // 左侧 0-35 像素放动态频谱
    UI_DrawDynamicSpectrum(2, 20, 32, 20);

    // 右侧 40-128 像素滚动歌名 (现在可以控制列了！)
    // X=40, Y=24, 宽度=88
    UI_DrawMixedScroll_Window(40, 24, 88, g_playlist.current_filename, now);

    // --- C. 底部区域 ---
    // 进度条
    OLED_DrawProgressBar(0, 44, 128, 4, (elapsed % 300) * 100 / 300);

    // 左下角：播放/暂停图标
    UI_DrawStatusIcon(4, 48, Audio_GetStatus());

    // 时间显示 (放在图标后面)
    snprintf(info_str, sizeof(info_str), "%02lu:%02lu / 05:00", elapsed / 60, elapsed % 60);
    OLED_ShowString(30, 48, info_str, 1);

    OLED_Update();
}