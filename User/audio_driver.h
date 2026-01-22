#ifndef __AUDIO_DRIVER_H
#define __AUDIO_DRIVER_H

#include "main.h"
#include "ff.h"    // <--- 核心修改：必须包含 FatFs 的头文件以识别 FRESULT
#include "fatfs.h" // 或者包含 CubeMX 生成的这个

// 音频缓冲区配置
#define AUDIO_BUF_SIZE    4096
#define MP3_IN_BUF_SIZE   4096

// 播放状态枚举
typedef enum {
    AUDIO_IDLE = 0,
    AUDIO_PLAYING,
    AUDIO_STOPPED,
    AUDIO_ERROR
} AudioStatus_t;

// 外部调用接口
void Audio_Init(void);
FRESULT Audio_Play(const char* filename);   // 此时 FRESULT 就能被识别了
void Audio_Stop(void);
void Audio_Process(void);
AudioStatus_t Audio_GetStatus(void);

#endif