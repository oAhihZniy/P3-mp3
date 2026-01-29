#ifndef __AUDIO_DRIVER_H
#define __AUDIO_DRIVER_H

#include "main.h"
#include "ff.h"    // <--- 核心修改：必须包含 FatFs 的头文件以识别 FRESULT
#include "fatfs.h" // 或者包含 CubeMX 生成的这个

// 音频缓冲区配置
#define AUDIO_BUF_SIZE    16384  // 16KB 总缓冲
#define MP3_IN_BUF_SIZE   8192
extern uint16_t AudioBuffer[AUDIO_BUF_SIZE / 2];
// 播放状态枚举
typedef enum {
    AUDIO_IDLE = 0,// 空闲
    AUDIO_PLAYING,// 播放中
    AUDIO_STOPPED,// 停止
    AUDIO_FINISHED,  // 代表当前歌曲自然播放完成
    AUDIO_ERROR// 错误
} AudioStatus_t;

// 外部调用接口
void Audio_Init(void);// 初始化音频系统
FRESULT Audio_Play(const char* filename);// 播放指定文件
void Audio_Stop(void);// 停止播放
void Audio_Process(void);// 音频处理主循环，需定期调用
AudioStatus_t Audio_GetStatus(void);// 获取当前播放状态

// static void Apply_Volume(uint16_t* buffer, uint32_t len);
void Audio_PauseResume(void);// 切换播放/暂停状态
void Audio_SetVolume(uint8_t vol);// 设置音量 (0-100)
uint8_t Audio_GetVolume(void);// 获取当前音量

uint32_t Audio_GetElapsedSec(void);// 获取已播放秒数
void Audio_ResetTimer(void);// 重置播放计时器


#endif