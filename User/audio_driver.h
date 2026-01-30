#ifndef __AUDIO_DRIVER_H
#define __AUDIO_DRIVER_H

#include "main.h"
#include "ff.h"
#include "mp3dec.h"

// --- 缓冲区配置 (核心修正) ---
// 1帧 MP3 (16bit立体声) = 1152 * 2 = 2304 个数据点
// 我们让半个缓冲区正好容纳 6 帧，总缓冲区容纳 12 帧
// 2304 * 12 = 27648
#define AUDIO_BUFFER_COUNT  27648

// 输入缓冲保持 8KB 不变
#define MP3_IN_BUF_SIZE     8192

// 状态机
typedef enum {
    AUDIO_IDLE = 0,
    AUDIO_PLAYING,
    AUDIO_STOPPED,
    AUDIO_FINISHED,
    AUDIO_ERROR
} AudioStatus_t;

extern MP3FrameInfo g_mp3FrameInfo;
// 导出给 main.c 做正弦波测试用
extern uint16_t AudioBuffer[AUDIO_BUFFER_COUNT];

// 接口
void Audio_Init(void);
FRESULT Audio_Play(const char* filename);
void Audio_Stop(void);
void Audio_PauseResume(void);
void Audio_Process(void);
AudioStatus_t Audio_GetStatus(void);
void Audio_SetVolume(uint8_t vol);
uint8_t Audio_GetVolume(void);
uint32_t Audio_GetElapsedSec(void);
void Audio_ResetTimer(void);
void Audio_Stream_Init(void);
#endif