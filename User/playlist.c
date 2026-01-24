#include "playlist.h"
#include "audio_driver.h"
#include <string.h>

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