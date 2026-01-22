#ifndef __AUDIO_DRIVER_H
#define __AUDIO_DRIVER_H

#include "main.h"

// 缓冲区大小定义
#define AUDIO_BUF_SIZE 4096 

// 导出变量和函数
extern uint16_t AudioBuffer[AUDIO_BUF_SIZE];
extern volatile uint8_t g_NextBufferHalf; // 0:不需要填充, 1:需要填前半部, 2:需要填后半部

void Audio_Start(void);
void Audio_Stop(void);
void Audio_GenerateTestTone(void);

#endif