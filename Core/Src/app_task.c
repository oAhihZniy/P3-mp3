//
// Created by shelter on 26-1-24.
//

#include "app_task.h"
#include "audio_driver.h"
#include "oled_driver.h"
#include "stm32f4xx_hal_gpio.h"

/* 建议放在定时器中断回调或 main 的 while(1) 配合 HAL_GetTick() */
void App_Task_Keyboard(void) {
    static uint32_t last_tick = 0;
    if (HAL_GetTick() - last_tick < 20) return; // 每 20ms 扫描一次
    last_tick = HAL_GetTick();

    // 播放/暂停键 (PA0)
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
        HAL_Delay(5); // 简单消抖
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
            Audio_PauseResume();
            while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET); // 等待松开
        }
    }

    // 音量加 (PA1)
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET) {
        Audio_SetVolume(Audio_GetVolume() + 5);
    }

    // 音量减 (PA2)
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET) {
        Audio_SetVolume(Audio_GetVolume() - 5);
    }

    // 下一曲 (PB0)
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) {
        // 切歌逻辑：停止当前 -> 寻找下一个文件 -> 播放
        // 这里需要文件列表管理逻辑
    }
}


/* main.c 或 app_task.c
 * 全局变量来控制滚动的“速度
 */
uint16_t title_scroll_offset = 0;
char current_title[] = "Burn My Dread -Mass Destruction-";

void UI_Refresh_Task(void) {
    OLED_Clear();

    // 1. 顶部状态栏
    OLED_ShowString(0, 0, "PLAYING", 1);

    // 2. 核心：滚动显示歌名
    // 每调用一次，offset 增加 1，实现每帧移动 1 像素
    OLED_ShowScrollString(0, 16, 128, current_title, title_scroll_offset++);

    // 3. 底部进度条
    OLED_DrawProgressBar(0, 28, 128, 4, 45);

    OLED_Update();
}