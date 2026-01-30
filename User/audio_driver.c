#include "audio_driver.h"
#include "i2s.h"
#include <string.h>

/* --- 全局变量 --- */
uint8_t mp3InBuf[MP3_IN_BUF_SIZE];
uint8_t *readPtr = mp3InBuf;
int bytesLeft = 0;
HMP3Decoder hMP3Decoder;
MP3FrameInfo mp3FrameInfo;

/* --- 私有控制变量 --- */
static FIL mp3File;
static AudioStatus_t audioStatus = AUDIO_IDLE;
static uint8_t volume = 40; // 默认音量 40%

// 统计
static uint32_t g_PlayedSamples = 0;
static uint32_t g_SampleRate = 44100;

// 输出缓冲区 (DMA用)
// 大小为 24KB (12288 个 uint16_t)
uint16_t AudioBuffer[AUDIO_BUF_SIZE / 2];

// DMA 双缓冲标志位
static volatile uint8_t fillBufferFlag = 0;

/* ================== 1. 核心底层逻辑 (保留你之前的) ================== */

void Audio_Stream_Init(void) {
    memset(mp3InBuf, 0, sizeof(mp3InBuf));
    readPtr = mp3InBuf;
    bytesLeft = 0;
}

int Audio_Fill_Stream(FIL* file) {
    UINT br = 0;
    if (bytesLeft > 0 && readPtr != mp3InBuf) {
        memmove(mp3InBuf, readPtr, bytesLeft);
    }
    readPtr = mp3InBuf;

    int space_available = MP3_IN_BUF_SIZE - bytesLeft;
    if (space_available >= 512) {
        // (我是牢理) 杜邦线防干扰保护
        // __disable_irq();
        FRESULT res = f_read(file, mp3InBuf + bytesLeft, space_available, &br);
        // __enable_irq();
        if (res != FR_OK) return -1;
        bytesLeft += br;
    }
    return br;
}

void Audio_Decoder_Init(void) {
    hMP3Decoder = MP3InitDecoder();
}

int Audio_Decode_Frame(int16_t *pcm_out) {
    int syncOffset, err;
    if (bytesLeft < 2048) return -1;

    syncOffset = MP3FindSyncWord(readPtr, bytesLeft);
    if (syncOffset < 0) {
        if (bytesLeft > (MP3_IN_BUF_SIZE - 512)) {
            readPtr += 1024; bytesLeft -= 1024;
        }
        return -2;
    }

    readPtr += syncOffset;
    bytesLeft -= syncOffset;

    err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, pcm_out, 0);

    if (err != ERR_MP3_NONE) {
        if (bytesLeft > 2) { readPtr += 2; bytesLeft -= 2; }
        else { bytesLeft = 0; }
        return err;
    }
    MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
    return 0;
}

void Audio_Skip_ID3(FIL* file) {
    uint8_t header[10];
    UINT br;
    f_lseek(file, 0);
    f_read(file, header, 10, &br);
    if (br == 10 && header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        uint32_t tag_size = ((header[6] & 0x7F) << 21) | ((header[7] & 0x7F) << 14) |
                            ((header[8] & 0x7F) << 7)  | (header[9] & 0x7F);
        f_lseek(file, tag_size + 10);
        bytesLeft = 0; readPtr = mp3InBuf;
    } else {
        f_lseek(file, 0);
    }
}

/* ================== 2. 核心中层逻辑 (新增：格式拉伸) ================== */

/**
 * (我是牢理) 将 Helix 的 16位紧凑数据，拉伸为 I2S 的 32位帧格式 (16数据+16补零)
 * 同时应用音量。
 * pcm_in: 既是输入也是输出 (AudioBuffer 的指针)
 * samples: 这一帧的总采样点数
 */
// static void Format_And_Volume(short* pcm_in, int samples) {
//     uint16_t* pOut = (uint16_t*)pcm_in;
//     // 必须逆向循环，防止覆盖
//     for (int i = samples - 1; i >= 0; i--) {
//         int32_t temp = (int32_t)pcm_in[i];
//         uint16_t val = (uint16_t)((temp * volume) / 100);
//
//         // [Data] [0] [Data] [0] ...
//         pOut[i * 2] = val;
//         pOut[i * 2 + 1] = 0;
//     }
// }
static void Format_And_Volume(short* pcm, int samples) {
    if (volume == 100) return; // 满音量直接跳过

    for (int i = 0; i < samples; i++) {
        int32_t temp = (int32_t)pcm[i];
        // 简单的线性音量缩放
        pcm[i] = (short)((temp * volume) / 100);
    }
}

/* ================== 3. 高层控制逻辑 (新增：播放与调度) ================== */

void Audio_Init(void) {
    Audio_Decoder_Init();
    Audio_Stream_Init();
    audioStatus = AUDIO_IDLE;
}

FRESULT Audio_Play(const char* filename) {
    Audio_Stop();

    FRESULT res = f_open(&mp3File, filename, FA_READ);
    if (res != FR_OK) {
        audioStatus = AUDIO_ERROR;
        return res;
    }

    Audio_Skip_ID3(&mp3File);
    Audio_Stream_Init(); // 重置缓冲区指针
    g_PlayedSamples = 0;
    memset(AudioBuffer, 0, sizeof(AudioBuffer));

    // 预解码两帧，填满缓冲区的一部分，防止启动噪音
    Audio_Fill_Stream(&mp3File); // 第一次先读满

    // (我是牢理) 这里的 0 是 AudioBuffer 的偏移量
    short* pStart = (short*)&AudioBuffer[0];
    if (Audio_Decode_Frame(pStart) == 0) {
        Format_And_Volume(pStart, mp3FrameInfo.outputSamps);
    }

    // 启动 DMA (Circular模式)
    // 传输长度 = 数组元素个数
    HAL_I2S_Transmit_DMA(&hi2s2, AudioBuffer, AUDIO_BUF_SIZE / 2);

    audioStatus = AUDIO_PLAYING;
    return FR_OK;
}

void Audio_Stop(void) {
    HAL_I2S_DMAStop(&hi2s2);
    f_close(&mp3File);
    audioStatus = AUDIO_STOPPED;
}

// 【核心调度器】放在 main while(1) 中
void Audio_Process(void) {
    if (audioStatus != AUDIO_PLAYING) return;

    // 尝试补货
    Audio_Fill_Stream(&mp3File);

    // 检查 DMA 是否需要新数据
    if (fillBufferFlag == 0) return;

    uint8_t half = fillBufferFlag;
    fillBufferFlag = 0;

    // 半区长度 (单位: uint16_t) -> 24KB / 2 / 2 = 6144
    uint32_t half_len = AUDIO_BUF_SIZE / 4;
    uint32_t start_pos = (half == 1) ? 0 : half_len;
    uint32_t end_pos = start_pos + half_len;
    uint32_t current_pos = start_pos;

    // (我是牢理) 贪婪填充循环
    // 4608 是拉伸后一帧占用的空间 (2304 * 2)
    while (current_pos < (end_pos - 2304)) {
        short* pOut = (short*)&AudioBuffer[current_pos];
        int err = Audio_Decode_Frame(pOut);

        if (err < 0) {
            if (err == -1) { // 读到文件尾
                Audio_Stop();
                audioStatus = AUDIO_FINISHED;
            }
            break;
        }

        // 格式化并拉伸数据
        Format_And_Volume(pOut, mp3FrameInfo.outputSamps);

        // 更新统计
        g_PlayedSamples += mp3FrameInfo.outputSamps / 2;
        g_SampleRate = mp3FrameInfo.samprate;

        // 指针后移 (采样数 * 2，因为拉伸了)
        current_pos += (mp3FrameInfo.outputSamps );
    }
}

// 辅助控制函数
void Audio_PauseResume(void) {
    if (audioStatus == AUDIO_PLAYING) {
        HAL_I2S_DMAPause(&hi2s2);
        audioStatus = AUDIO_STOPPED; // 标记为停止，防止 Process 继续填
    } else if (audioStatus == AUDIO_STOPPED) {
        HAL_I2S_DMAResume(&hi2s2);
        audioStatus = AUDIO_PLAYING;
    }
}

void Audio_SetVolume(uint8_t vol) {
    volume = (vol > 100) ? 100 : vol;
}

uint8_t Audio_GetVolume(void) { return volume; }
AudioStatus_t Audio_GetStatus(void) { return audioStatus; }
uint32_t Audio_GetElapsedSec(void) { return (g_SampleRate > 0) ? g_PlayedSamples / g_SampleRate : 0; }
void Audio_ResetTimer(void) { g_PlayedSamples = 0; }

/* ================== 中断回调 ================== */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) fillBufferFlag = 1; // 填前半部
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) fillBufferFlag = 2; // 填后半部
}