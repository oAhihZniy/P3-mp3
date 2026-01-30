#ifndef __AUDIO_DRIVER_H
#define __AUDIO_DRIVER_H

#include "main.h"
#include "ff.h"
#include "mp3dec.h"

// --- 缓冲区配置 ---
// 16-bit on 32-bit 模式：1帧 MP3 (2304采样) 拉伸后占用 9216 字节
// 设定总缓冲 24KB (每半区 12KB)，足够容纳任何一帧并留有余量
#define AUDIO_BUF_SIZE    24576
#define MP3_IN_BUF_SIZE   8192

// --- 播放状态枚举 ---
typedef enum {
    AUDIO_IDLE = 0,
    AUDIO_PLAYING,
    AUDIO_STOPPED,
    AUDIO_FINISHED, // 播放自然结束
    AUDIO_ERROR
} AudioStatus_t;

// --- 变量导出 ---
extern MP3FrameInfo mp3FrameInfo;
extern uint8_t mp3InBuf[MP3_IN_BUF_SIZE];
extern uint8_t *readPtr;
extern int bytesLeft;
// 导出 AudioBuffer 供 main.c 的正弦波测试用 (如果需要)
extern uint16_t AudioBuffer[AUDIO_BUF_SIZE / 2];

// --- 函数接口 ---
// 1. 初始化与底层
void Audio_Init(void);
void Audio_Stream_Init(void);
int Audio_Fill_Stream(FIL* file);
void Audio_Decoder_Init(void);
int Audio_Decode_Frame(int16_t *pcm_out);
void Audio_Skip_ID3(FIL* file);

// 2. 高层控制 (你缺少的)
FRESULT Audio_Play(const char* filename);
void Audio_Stop(void);
void Audio_PauseResume(void);
void Audio_Process(void); // 主循环任务

// 3. 状态与设置
AudioStatus_t Audio_GetStatus(void);
void Audio_SetVolume(uint8_t vol);
uint8_t Audio_GetVolume(void);
uint32_t Audio_GetElapsedSec(void);
void Audio_ResetTimer(void);

#endif