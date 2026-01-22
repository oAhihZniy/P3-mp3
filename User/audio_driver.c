#include "audio_driver.h"
#include "i2s.h"

// 缓冲区大小 (必须是 4 的倍数，因为是 16bit 双声道)
#define AUDIO_BUF_SIZE 4096

// 音频缓冲区
uint16_t AudioBuffer[AUDIO_BUF_SIZE];

// 播放状态枚举
typedef enum {
    AUDIO_STATE_IDLE,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_STOP
} AudioState_t;

volatile AudioState_t g_AudioState = AUDIO_STATE_IDLE;

// 1. 启动播放
void Audio_Start(void) {
    // 初始化缓冲区为静音 (0)
    for(int i=0; i<AUDIO_BUF_SIZE; i++) AudioBuffer[i] = 0;

    g_AudioState = AUDIO_STATE_PLAYING;
    // 启动 DMA 循环传输
    // 注意：第三个参数是数据量，uint16_t 传输次数是 AUDIO_BUF_SIZE
    HAL_I2S_Transmit_DMA(&hi2s2, AudioBuffer, AUDIO_BUF_SIZE);
}

// 2. 停止播放
void Audio_Stop(void) {
    HAL_I2S_DMAStop(&hi2s2);
    g_AudioState = AUDIO_STATE_STOP;
}

// 3. 关键回调：DMA 传输到一半 (前半部发完了)
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    if(hi2s->Instance == SPI2) { // I2S2 在内部对应 SPI2
        // 这里设置标志位，通知主循环：快把新的 MP3 解码数据填入 AudioBuffer[0 ... AUDIO_BUF_SIZE/2 - 1]
        // 稍后在集成 Helix 库时会用到
    }
}

// 4. 关键回调：DMA 传输完成 (后半部发完了)
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
    if(hi2s->Instance == SPI2) {
        // 这里设置标志位，通知主循环：快把新的 MP3 解码数据填入 AudioBuffer[AUDIO_BUF_SIZE/2 ... AUDIO_BUF_SIZE - 1]
    }
}
