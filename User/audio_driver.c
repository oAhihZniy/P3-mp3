#include "audio_driver.h"
#include "fatfs.h"
#include "mp3dec.h"
#include "i2s.h"
#include <string.h>

/* --- 私有变量 --- */
static HMP3Decoder hMP3Decoder;
static MP3FrameInfo mp3FrameInfo;
static FIL mp3File;
static AudioStatus_t audioStatus = AUDIO_IDLE;
// 定义音量，范围 0~100
static uint8_t volume = 30;// 默认音量 30%

// 缓冲区定义
uint16_t AudioBuffer[AUDIO_BUF_SIZE / 2]; // 16-bit PCM 数组
static uint8_t mp3InBuf[MP3_IN_BUF_SIZE]; // MP3 原始数据缓冲
static uint8_t *readPtr = mp3InBuf;       // 当前解码位置指针
static int bytesLeft = 0;                 // 缓冲区剩余未解数据字节数

// 标志位：0-无动作, 1-需填充前半部, 2-需填充后半部
static volatile uint8_t fillBufferFlag = 0;

/* --- 内部私有函数 --- */

/**
 * 从 SD 卡读取数据填充 MP3 输入缓冲区
 */
static int Fill_MP3_InputBuffer(void) {
    UINT br;
    // 1. 将剩余数据移到开头
    if (bytesLeft > 0 && readPtr != mp3InBuf) {
        memmove(mp3InBuf, readPtr, bytesLeft);
    }
    readPtr = mp3InBuf;

    // 2. 从 SD 卡读取新数据填满剩余空间
    FRESULT res = f_read(&mp3File, mp3InBuf + bytesLeft, MP3_IN_BUF_SIZE - bytesLeft, &br);
    if (res != FR_OK) return -1;

    bytesLeft += br;
    return br;
}

/**
 * 内部函数：调节 PCM 数据的音量
 */
static void Apply_Volume(short* pcm_samples, int num_samples) {
    if (volume == 100) return; // 100% 音量无需计算，节省 CPU

    for (int i = 0; i < num_samples; i++) {
        // 使用 32 位整数中转运算，防止 16 位溢出和精度丢失
        int32_t temp = (int32_t)pcm_samples[i];
        pcm_samples[i] = (short)((temp * volume) / 100);
    }
}

/**
 * 解码一帧 MP3 并写入对应的 PCM 缓冲区位置
 * offset: PCM 缓冲区的起始位置 (0 或 AUDIO_BUF_SIZE/4)
 */
static int Decode_Next_Frame(uint32_t offset) {
    // 数据量不足一帧，尝试读取
    if (bytesLeft < 1024) {
        if (Fill_MP3_InputBuffer() <= 0 && bytesLeft == 0) return -1; // 文件结束
    }

    // 寻找同步字
    int syncOffset = MP3FindSyncWord(readPtr, bytesLeft);
    if (syncOffset < 0) {
        bytesLeft = 0; // 丢弃无效数据
        return -2;
    }
    readPtr += syncOffset;
    bytesLeft -= syncOffset;

    // 解码到 PCM 缓冲区 (注意指针偏移和数据转换)
    short *outPtr = (short *)&AudioBuffer[offset];
    int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, outPtr, 0);

    if (err == ERR_MP3_NONE) {
        MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
        // 如果这里采样率与 I2S 配置不符，可以动态调整 I2S 时钟 (进阶功能)

        // 【核心点】在这里立即调节音量
        // 参数 1：当前写入的缓冲区首地址
        // 参数 2：这一帧输出的采样总数 (左右声道合计)
        Apply_Volume(outPtr, mp3FrameInfo.outputSamps);
    }
    return err;
}

/* --- 外部公共接口 --- */

void Audio_Init(void) {
    hMP3Decoder = MP3InitDecoder();
    audioStatus = AUDIO_IDLE;
}

FRESULT Audio_Play(const char* filename) {// 开始播放指定文件
    Audio_Stop();

    FRESULT res = f_open(&mp3File, filename, FA_READ);
    if (res != FR_OK) {
        audioStatus = AUDIO_ERROR;
        return res;
    }

    // 重置指针和状态
    bytesLeft = 0;
    readPtr = mp3InBuf;
    fillBufferFlag = 0;
    memset(AudioBuffer, 0, sizeof(AudioBuffer));

    // 预解码前两块，填满缓冲区
    Decode_Next_Frame(0);
    Decode_Next_Frame(AUDIO_BUF_SIZE / 4);

    // 启动 I2S DMA 传输 (Circular 模式)
    // 参数 3 是传输次数，16-bit 模式下为数组长度
    HAL_I2S_Transmit_DMA(&hi2s2, AudioBuffer, sizeof(AudioBuffer)/2);

    audioStatus = AUDIO_PLAYING;
    return FR_OK;
}

void Audio_Stop(void) {// 停止播放
    HAL_I2S_DMAStop(&hi2s2);
    f_close(&mp3File);
    audioStatus = AUDIO_STOPPED;
}

void Audio_Process(void) {// 主循环中调用，处理缓冲区填充
    if (audioStatus != AUDIO_PLAYING) return;

    if (fillBufferFlag == 1) { // 需填前半部
        if (Decode_Next_Frame(0) < 0) {
            Audio_Stop(); // 播放结束
        }
        fillBufferFlag = 0;
    }
    else if (fillBufferFlag == 2) { // 需填后半部
        if (Decode_Next_Frame(AUDIO_BUF_SIZE / 4) < 0) {
            Audio_Stop(); // 播放结束
        }
        fillBufferFlag = 0;
    }
}

AudioStatus_t Audio_GetStatus(void) {// 获取当前播放状态
    return audioStatus;
}

/* --- 中断回调函数 --- */

// DMA 传输过半 (开始播后半部，需填前半部)
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) {
        fillBufferFlag = 1;
    }
}

// DMA 传输完成 (开始播前半部，需填后半部)
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) {
        fillBufferFlag = 2;
    }
}


/**
 * 暂停/恢复切换
 */
void Audio_PauseResume(void) {
    if (audioStatus == AUDIO_PLAYING) {
        HAL_I2S_DMAPause(&hi2s2); // 硬件层暂停 DMA
        audioStatus = AUDIO_STOPPED;
    } else if (audioStatus == AUDIO_STOPPED) {
        HAL_I2S_DMAResume(&hi2s2); // 硬件层恢复 DMA
        audioStatus = AUDIO_PLAYING;
    }
}

/**
 * 设置音量接口
 */
void Audio_SetVolume(uint8_t vol) {
    if (vol > 100) vol = 100;
    volume = vol;
}

uint8_t Audio_GetVolume(void) {
    return volume;
}