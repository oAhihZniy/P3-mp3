#ifndef OLED_APP_H
#define OLED_APP_H

#include <stdint.h>

// UI 核心任务
void UI_Refresh_Task(void);

// 混合文字滚动逻辑
void UI_DrawMixedScrollTitle(uint8_t y, const char* str, uint32_t tick);


//测试
void UI_Test_Task(void);
#endif