#include "playlist.h"

#include <stdio.h>

#include "audio_driver.h"
#include <string.h>

#include "oled_driver.h"

Playlist_t g_playlist = {0, 0, ""};

/**
 * 扫描根目录所有 .mp3 文件，统计总数
 */
FRESULT Playlist_Init(void) {
    DIR dir;
    FILINFO fno;
    FRESULT res;

    g_playlist.total_count = 0;
    res = f_opendir(&dir, "/"); // 打开根目录
    if (res != FR_OK) return res;

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break; // 错误或结束

        // 检查后缀是否为 .mp3
        if (!(fno.fattrib & AM_DIR)) { // 不是目录
            char *fn = fno.fname;
            int len = strlen(fn);
            if (len > 4 && strcasecmp(fn + len - 4, ".mp3") == 0) {
                g_playlist.total_count++;
                if (g_playlist.total_count >= MAX_SONGS) break;
            }
        }
    }
    f_closedir(&dir);
    return FR_OK;
}

/**
 * 根据索引号在 SD 卡中寻找文件名
 */
FRESULT Playlist_GetFileName(uint16_t index, char* out_name) {
    DIR dir;
    FILINFO fno;
    FRESULT res;
    uint16_t count = 0;

    res = f_opendir(&dir, "/");
    if (res != FR_OK) return res;

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        if (!(fno.fattrib & AM_DIR)) {
            int len = strlen(fno.fname);
            if (len > 4 && strcasecmp(fno.fname + len - 4, ".mp3") == 0) {
                if (count == index) {
                    strcpy(out_name, fno.fname);
                    f_closedir(&dir);
                    return FR_OK;
                }
                count++;
            }
        }
    }
    f_closedir(&dir);
    return FR_NO_FILE;
}

void Playlist_PlayCurrent(void) {
    if (Playlist_GetFileName(g_playlist.current_index, g_playlist.current_filename) == FR_OK) {
        Audio_Play(g_playlist.current_filename);
    }
}

void Playlist_Next(void) {
    g_playlist.current_index++;
    if (g_playlist.current_index >= g_playlist.total_count) {
        g_playlist.current_index = 0; // 回到第一首
    }
    Playlist_PlayCurrent();
}

void Playlist_Prev(void) {
    if (g_playlist.current_index == 0) {
        g_playlist.current_index = g_playlist.total_count - 1;
    } else {
        g_playlist.current_index--;
    }
    Playlist_PlayCurrent();
}

/**
 * 自动切歌检查
 * 建议在处理完后将状态回归 IDLE，防止重复触发
 */
void Playlist_AutoNext_Task(void) {
    if (Audio_GetStatus() == AUDIO_FINISHED) {
        // 播放下一首之前，先确保状态不再是 FINISHED
        Playlist_Next();
    }
}

// 根目录全文件扫描函数
// void Debug_Scan_All_Files(void) {
//     DIR dir;
//     FILINFO fno;
//     FRESULT res;
//     int line = 0;
//
//     OLED_Clear();
//     OLED_ShowString(0, 0, "-- ROOT FILES --", 1);
//     OLED_Update();
//
//     // 1. 打开根目录
//     res = f_opendir(&dir, "/");
//     if (res != FR_OK) {
//         OLED_ShowString(0, 16, "OPEN DIR FAIL", 1);
//         OLED_Update();
//         return;
//     }
//
//     // 2. 循环读取目录项
//     while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
//         // 最多显示 3 行（留下标题行）
//         if (line < 5) {
//             char display_buf[32];
//             // 如果是文件夹，加个[D]前缀
//             if (fno.fattrib & AM_DIR) {
//                 snprintf(display_buf, sizeof(display_buf), "[D] %s", fno.fname);
//             } else {
//                 snprintf(display_buf, sizeof(display_buf), "    %s", fno.fname);
//             }
//
//             // 关键：这里使用你的中日韩混合显示函数
//             // 假设每一行 16 像素高
//             OLED_ShowSDString(0, (int16_t)(-16 + (line * 16)), display_buf);
//             line++;
//         } else {
//             // 如果文件太多，屏幕放不下了
//             OLED_ShowString(0, 48, "MORE FILES...", 1);
//             break;
//         }
//     }
//     f_closedir(&dir);
//     OLED_Update();
//  }