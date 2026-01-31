#ifndef __AUDIO_DRIVER_H
#define __AUDIO_DRIVER_H

#include "main.h"
#include "ff.h"
#include "mp3dec.h"

// --- 缓冲区配置  ---
// 1帧 MP3 (16bit立体声) = 1152 * 2 = 2304 个数据点
// 让半个缓冲区正好容纳 6 帧，总缓冲区容纳 12 帧
// 2304 * 12 = 27648
#define AUDIO_BUFFER_COUNT  2304*6
// 输入缓冲保持 8KB 不变
#define MP3_IN_BUF_SIZE     8192

extern uint32_t g_total_duration; // 总时长（秒）
extern uint32_t g_file_size;      // 文件大小
// 状态机
typedef enum {
    AUDIO_IDLE = 0,
    AUDIO_PLAYING,
    AUDIO_STOPPED,
    AUDIO_FINISHED,
    AUDIO_ERROR
} AudioStatus_t;

extern MP3FrameInfo g_mp3FrameInfo;
//  main.c 做正弦波测试用
extern uint16_t AudioBuffer[AUDIO_BUFFER_COUNT];

// 接口
void Audio_Set_I2S_Freq(uint32_t freq);
void Audio_Init(void);// 音频系统初始化
FRESULT Audio_Play(const char* filename);// 播放指定文件
void Audio_Stop(void);// 停止播放
void Audio_PauseResume(void);// 播放/暂停切换
void Audio_Process(void);// 音频核心处理函数，建议放在主循环最高优先级调用
AudioStatus_t Audio_GetStatus(void);// 获取当前状态
void Audio_SetVolume(uint8_t vol);// 设置音量 0~100
uint8_t Audio_GetVolume(void);// 获取音量
uint32_t Audio_GetElapsedSec(void);// 获取已播放秒数
void Audio_ResetTimer(void);// 重置播放时间计数
void Audio_Stream_Init(void);// 初始化/复位码流缓冲区指针
#endif