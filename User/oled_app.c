#include "oled_app.h"
#include "oled_driver.h"
#include "audio_driver.h"
#include "playlist.h"
#include <string.h>
#include <stdio.h>

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

void UI_Refresh_Task(void) {
    char info_str[32];
    uint32_t elapsed = Audio_GetElapsedSec();

    OLED_Clear();

    // 1. 顶部：索引和播放状态 (P3 风格)
    // 格式: "01/15  >" (播放) 或 "01/15  ||" (暂停)
    snprintf(info_str, sizeof(info_str), "%02d/%02d  %s",
             g_playlist.current_index + 1, g_playlist.total_count,
             (Audio_GetStatus() == AUDIO_PLAYING) ? ">" : "||");
    OLED_ShowString(0, 0, info_str, 1);

    // 2. 中部：中日韩混合滚动歌名
    UI_DrawMixedScrollTitle(12, g_playlist.current_filename, HAL_GetTick());

    // 3. 底部：进度条 + 播放时间
    OLED_DrawProgressBar(0, 28, 90, 4, 0); // 进度目前设为0，后续可根据时长计算

    snprintf(info_str, sizeof(info_str), "%02lu:%02lu", elapsed / 60, elapsed % 60);
    OLED_ShowString(40, 32, info_str, 1);

    OLED_Update();
}



// 测试界面任务
// 在 oled_app.c 顶部定义两个模拟变量
static uint32_t test_seconds = 0;
static uint32_t test_last_tick = 0;

void UI_Test_Task(void) {
    char info_str[32];
    uint32_t current_tick = HAL_GetTick();

    // 1. 【核心修正】模拟时间自增逻辑，不依赖音频驱动
    if (current_tick - test_last_tick >= 1000) {
        test_last_tick = current_tick;
        test_seconds++;
    }

    OLED_Clear();

    // 2. 顶部状态
    snprintf(info_str, sizeof(info_str), "01/10  MOCK >");
    OLED_ShowString(0, 0, info_str, 1);

    // 3. 中部：平滑滚动歌名 (tick 传 current_tick)
    UI_DrawMixedScrollTitle(20, "Burn My Dread -Persona 3- Testing...", current_tick);

    // 4. 底部：进度条 (根据模拟时间算进度，假设歌曲长100秒)
    OLED_DrawProgressBar(0, 42, 120, 6, (test_seconds % 100));

    // 5. 底部时间：显示模拟时间
    snprintf(info_str, sizeof(info_str), "%02lu:%02lu/01:40", test_seconds / 60, test_seconds % 60);
    OLED_ShowString(30, 50, info_str, 1);

    OLED_Update();
}




