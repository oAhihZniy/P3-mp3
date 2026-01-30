#include "audio_driver.h"
#include "i2s.h"
#include <string.h>

/* --- 变量定义 --- */
uint16_t AudioBuffer[AUDIO_BUFFER_COUNT]; // 27648 * 2 = 54KB RAM (F411 够用)
static uint8_t mp3InBuf[MP3_IN_BUF_SIZE];
static uint8_t *readPtr = mp3InBuf;
static int bytesLeft = 0;

static HMP3Decoder hMP3Decoder;
static FIL mp3File;
MP3FrameInfo g_mp3FrameInfo;

static volatile uint8_t fillBufferFlag = 0;
static AudioStatus_t audioStatus = AUDIO_IDLE;
static uint8_t currentVolume = 5;
static uint32_t playedSamples = 0;
static uint32_t currentSampleRate = 44100;

/* --- 内部函数 --- */

static void Apply_Volume(int16_t* pcm, int samples) {
    if (currentVolume >= 100) return;
    for (int i = 0; i < samples; i++) {
        int32_t temp = pcm[i];
        pcm[i] = (int16_t)((temp * currentVolume) / 100);
    }
}

static int Fill_Stream_Buffer(void) {
    UINT br;
    if (bytesLeft > 0 && readPtr != mp3InBuf) {
        memmove(mp3InBuf, readPtr, bytesLeft);
    }
    readPtr = mp3InBuf;

    int space = MP3_IN_BUF_SIZE - bytesLeft;
    if (space < 512) return 0;

    FRESULT res = f_read(&mp3File, mp3InBuf + bytesLeft, space, &br);


    if (res != FR_OK) return -1;
    bytesLeft += br;
    return br;
}

static void Skip_ID3_Tag(void) {
    uint8_t header[10];
    UINT br;
    f_lseek(&mp3File, 0);
    f_read(&mp3File, header, 10, &br);
    if (br == 10 && header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        uint32_t size = ((header[6] & 0x7F) << 21) | ((header[7] & 0x7F) << 14) |
                        ((header[8] & 0x7F) << 7)  | (header[9] & 0x7F);
        f_lseek(&mp3File, size + 10);
    } else {
        f_lseek(&mp3File, 0);
    }
    bytesLeft = 0; readPtr = mp3InBuf;
}

static int Decode_Frame_To_Buffer(int16_t *target_buf) {
    if (bytesLeft < 2048) {
        if (Fill_Stream_Buffer() <= 0 && bytesLeft == 0) return -1;
    }

    int offset = MP3FindSyncWord(readPtr, bytesLeft);
    if (offset < 0) {
        if (bytesLeft > 1024) { readPtr += 1024; bytesLeft -= 1024; }
        else { bytesLeft = 0; }
        return -2;
    }

    readPtr += offset;
    bytesLeft -= offset;

    int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, target_buf, 0);

    if (err == ERR_MP3_NONE) {
        MP3GetLastFrameInfo(hMP3Decoder, &g_mp3FrameInfo);
        Apply_Volume(target_buf, g_mp3FrameInfo.outputSamps);
        playedSamples += g_mp3FrameInfo.outputSamps / 2;
        return 0;
    } else {
        if (bytesLeft > 2) { readPtr += 2; bytesLeft -= 2; }
        else bytesLeft = 0;
        return err;
    }
}

/* --- 核心处理逻辑 (完美对齐版) --- */

void Audio_Process(void) {
    if (audioStatus != AUDIO_PLAYING || fillBufferFlag == 0) return;

    uint8_t part = fillBufferFlag;
    fillBufferFlag = 0;

    // 半区大小 (单位: 采样点个数)
    // 27648 / 2 = 13824
    // 13824 / 2304 = 6.0 帧 (整除！完美！)
    uint32_t half_len = AUDIO_BUFFER_COUNT / 2;
    uint32_t start_pos = (part == 1) ? 0 : half_len;
    uint32_t end_pos = start_pos + half_len;
    uint32_t current_pos = start_pos;

    // 循环填充：因为是整数倍，所以一定会刚好填满，不会溢出
    while (current_pos < end_pos) {
        int err = Decode_Frame_To_Buffer((int16_t*)&AudioBuffer[current_pos]);

        if (err == 0) {
            current_pos += g_mp3FrameInfo.outputSamps;
        } else if (err == -1) {
            Audio_Stop();
            audioStatus = AUDIO_FINISHED;
            break;
        } else {
            // 解码错，不移动指针，或者填静音跳过
            // 这里简单处理：跳过一帧的长度填0
            // memset(&AudioBuffer[current_pos], 0, 2304*2);
            // current_pos += 2304;
        }
    }
}

/* --- 外部接口 --- */

void Audio_Init(void) {
    hMP3Decoder = MP3InitDecoder();
    audioStatus = AUDIO_IDLE;
}

FRESULT Audio_Play(const char* filename) {
    Audio_Stop();
    FRESULT res = f_open(&mp3File, filename, FA_READ);
    if (res != FR_OK) { audioStatus = AUDIO_ERROR; return res; }

    Skip_ID3_Tag();
    Audio_Stream_Init(); // 重置缓冲区
    playedSamples = 0;
    memset(AudioBuffer, 0, sizeof(AudioBuffer));
    fillBufferFlag = 0;

    // 预填满两个半区 (共12帧)
    uint32_t pos = 0;
    // 填满整个缓冲区
    while (pos < AUDIO_BUFFER_COUNT) {
        if (Decode_Frame_To_Buffer((int16_t*)&AudioBuffer[pos]) == 0) {
            pos += g_mp3FrameInfo.outputSamps;
        } else {
            break;
        }
    }

    // 启动 DMA
    // 长度参数是 uint16_t 的总数
    HAL_I2S_Transmit_DMA(&hi2s2, AudioBuffer, AUDIO_BUFFER_COUNT);

    audioStatus = AUDIO_PLAYING;
    return FR_OK;
}

void Audio_Stop(void) {
    HAL_I2S_DMAStop(&hi2s2);
    f_close(&mp3File);
    audioStatus = AUDIO_STOPPED;
}

void Audio_PauseResume(void) {
    if (audioStatus == AUDIO_PLAYING) {
        HAL_I2S_DMAPause(&hi2s2);
        audioStatus = AUDIO_STOPPED;
    } else if (audioStatus == AUDIO_STOPPED) {
        HAL_I2S_DMAResume(&hi2s2);
        audioStatus = AUDIO_PLAYING;
    }
}
/**
 * (我是牢理) 初始化/复位码流缓冲区指针
 */
void Audio_Stream_Init(void) {
    bytesLeft = 0;
    readPtr = mp3InBuf;
    memset(mp3InBuf, 0, sizeof(mp3InBuf));
}
void Audio_SetVolume(uint8_t vol) { currentVolume = (vol > 100) ? 100 : vol; }
uint8_t Audio_GetVolume(void) { return currentVolume; }
AudioStatus_t Audio_GetStatus(void) { return audioStatus; }
uint32_t Audio_GetElapsedSec(void) { return (currentSampleRate > 0) ? playedSamples / currentSampleRate : 0; }
void Audio_ResetTimer(void) { playedSamples = 0; }

/* --- 中断 --- */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) fillBufferFlag = 1;
}
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) fillBufferFlag = 2;
}