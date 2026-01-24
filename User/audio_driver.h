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
    AUDIO_IDLE = 0,// 空闲
    AUDIO_PLAYING,// 播放中
    AUDIO_STOPPED,// 停止
    AUDIO_ERROR// 错误
} AudioStatus_t;

// 外部调用接口
void Audio_Init(void);
FRESULT Audio_Play(const char* filename);
void Audio_Stop(void);
void Audio_Process(void);
AudioStatus_t Audio_GetStatus(void);

// static void Apply_Volume(uint16_t* buffer, uint32_t len);
void Audio_PauseResume(void);// 切换播放/暂停状态
void Audio_SetVolume(uint8_t vol);// 设置音量 (0-100)
uint8_t Audio_GetVolume(void);// 获取当前音量

uint32_t Audio_GetElapsedSec(void);
void Audio_ResetTimer(void);

#endif