#ifndef __AUDIO_DRIVER_H
#define __AUDIO_DRIVER_H

#include "main.h"
#include "ff.h"
#include "mp3dec.h"

/* --- 配置参数 --- */
// 输出缓冲区：16KB (8192个双字节采样点)
// 半区 4096 个采样点，足以容纳一帧 MP3 (2304采样)
#define AUDIO_BUF_SIZE    16384*4
// 输入缓冲区：8KB (减少 SD 卡读取频次)
#define MP3_IN_BUF_SIZE   8196*2

/* --- 状态机 --- */
typedef enum {
    AUDIO_IDLE = 0,     // 未初始化或空闲
    AUDIO_PLAYING,      // 正在播放
    AUDIO_STOPPED,      // 用户暂停/停止
    AUDIO_FINISHED,     // 文件自然播放结束
    AUDIO_ERROR         // 发生硬件错误
} AudioStatus_t;

/* --- 全局变量导出 (供 main.c 调试用) --- */
extern MP3FrameInfo g_mp3FrameInfo;
extern uint16_t AudioBuffer[AUDIO_BUF_SIZE / 2]; // I2S DMA 缓冲区

/* --- API 接口 --- */
// 1. 系统级
void Audio_Init(void);
void Audio_Process(void); // 【核心】主循环任务

// 2. 播放控制
FRESULT Audio_Play(const char* filename);
void Audio_Stop(void);
void Audio_PauseResume(void);

// 3. 参数设置
AudioStatus_t Audio_GetStatus(void);
void Audio_SetVolume(uint8_t vol); // 0-100
uint8_t Audio_GetVolume(void);
uint32_t Audio_GetElapsedSec(void);
void Audio_ResetTimer(void);

#endif