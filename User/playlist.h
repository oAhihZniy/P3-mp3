#ifndef __PLAYLIST_H
#define __PLAYLIST_H

#include "ff.h"

// 最大支持 255 首歌，文件名最大 255 字节
#define MAX_SONGS 255
#define MAX_FILENAME_LEN 255

typedef struct {
    uint16_t total_count;    // 总歌曲数
    uint16_t current_index;  // 当前播放索引
    char current_filename[MAX_FILENAME_LEN]; // 当前播的文件名
} Playlist_t;

// 接口
FRESULT Playlist_Init(void);             // 扫描 SD 卡建立索引
FRESULT Playlist_GetFileName(uint16_t index, char* out_name); // 获取第 N 首歌的名字
void Playlist_Next(void);                // 下一曲
void Playlist_Prev(void);                // 上一曲
void Playlist_AutoNext_Task(void);       // 自动切歌检查逻辑
void Playlist_PlayCurrent(void);// 播放当前索引的歌曲
extern Playlist_t g_playlist;

#endif