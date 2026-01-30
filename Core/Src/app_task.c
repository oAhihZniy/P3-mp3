#include "app_task.h"
#include "audio_driver.h"
#include "playlist.h"
#include "oled_driver.h"

// 建议 20ms 调用一次
void App_Task_Keyboard(void) {
    static uint32_t last_tick = 0;
    if (HAL_GetTick() - last_tick < 20) return;
    last_tick = HAL_GetTick();

    // --- 1. 播放/暂停键 (PA0) - 状态机消抖 ---
    static uint8_t play_cnt = 0;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
        if (play_cnt < 3) play_cnt++;
        if (play_cnt == 2) { // 连读两次低电平即视为有效点击
            Audio_PauseResume();
        }
    } else {
        play_cnt = 0;
    }

    // --- 2. 音量控制 (PA1, PA2) - 允许长按连续调节 ---
    // 这里不需要松手检测，按住就会一直慢慢变
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET) { // Vol +
        uint8_t v = Audio_GetVolume();
        if (v < 100) {
            Audio_SetVolume(Audio_GetVolume() + 1);
        }
    }
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET) { // Vol -
        uint8_t v = Audio_GetVolume();
        if (v > 0) {
            Audio_SetVolume(Audio_GetVolume() - 1);
        }else {
            Audio_SetVolume(0);
        }
    }

    // --- 3. 下一曲 (PB0) - 状态机消抖 ---
    static uint8_t next_cnt = 0;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) {
        if (next_cnt < 3) next_cnt++;
        if (next_cnt == 2) {
            Playlist_Next(); // 调用之前写的切歌逻辑
        }
    } else {
        next_cnt = 0;
    }

    // --- 4. 上一曲 (假设在 PB3，根据你之前的规划) ---
    static uint8_t prev_cnt = 0;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
        if (prev_cnt < 3) prev_cnt++;
        if (prev_cnt == 2) {
            Playlist_Prev();
        }
    } else {
        prev_cnt = 0;
    }

    // --- 5. 耳机插拔实时检测 (PB1) ---
    // 即使没有中断，每 20ms 检测一次也足够快了
    static uint8_t last_hp_state = 0;
    uint8_t current_hp_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
    if (current_hp_state == GPIO_PIN_SET && last_hp_state == GPIO_PIN_RESET) {
        // 检测到耳机拔出 (假设高电平为拔出)
        if (Audio_GetStatus() == AUDIO_PLAYING) {
            Audio_PauseResume(); // 自动暂停
        }
    }
    last_hp_state = current_hp_state;
}