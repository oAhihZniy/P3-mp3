#include "audio_driver.h"
#include "i2s.h"
#include <string.h>

/* ================== 私有变量定义 ================== */

// --- 核心对象 ---
static HMP3Decoder hMP3Decoder;
static FIL mp3File;
MP3FrameInfo g_mp3FrameInfo; // 导出给外部看信息的

// --- 缓冲区管理 ---
// I2S DMA 输出缓冲区 (16bit PCM)
uint16_t AudioBuffer[AUDIO_BUF_SIZE / 2];

// MP3 码流输入缓冲区
static uint8_t mp3InBuf[MP3_IN_BUF_SIZE];
static uint8_t *readPtr = mp3InBuf;
static int bytesLeft = 0;

// --- 控制标志 ---
static volatile uint8_t fillBufferFlag = 0; // 0:空闲, 1:填前半, 2:填后半
static AudioStatus_t audioStatus = AUDIO_IDLE;
static uint8_t currentVolume = 6;

// --- 统计数据 ---
static uint32_t playedSamples = 0;
static uint32_t currentSampleRate = 44100;

/* ================== 内部辅助函数 ================== */

/**
 * (我是牢理) 动态调整 I2S 硬件采样率
 * 当 MP3 从 44.1k 变成 48k 时自动适配
 */
static void Audio_Set_I2S_Freq(uint32_t freq) {
    if (hi2s2.Init.AudioFreq != freq) {
        HAL_I2S_DMAStop(&hi2s2);  // 必须先停机
        hi2s2.Init.AudioFreq = freq;
        if (HAL_I2S_Init(&hi2s2) != HAL_OK) {
            audioStatus = AUDIO_ERROR;
        }
        currentSampleRate = freq;
    }
}

/**
 * 软件音量控制 (原位运算，不拉伸)
 * 针对 16 bits Data on 16 bits Frame 模式
 */
static void Apply_Volume(int16_t* pcm, int samples) {
    if (currentVolume >= 100) return; // 满音量不计算

    for (int i = 0; i < samples; i++) {
        int32_t temp = pcm[i];
        pcm[i] = (int16_t)((temp * currentVolume) / 100);
    }
}

/**
 * 补充 MP3 码流数据
 */
extern uint32_t real_time_bytes;// 引用 main.c 里的变量 用完记得删
static int Fill_Stream_Buffer(void) {
    UINT br;
    // 1. 数据搬运：把没解完的尾巴搬到头部
    if (bytesLeft > 0 && readPtr != mp3InBuf) {
        memmove(mp3InBuf, readPtr, bytesLeft);
    }
    readPtr = mp3InBuf;

    // 2. 计算空余空间
    int space = MP3_IN_BUF_SIZE - bytesLeft;
    if (space < 512) return 0; // 空间太小不读

    // 3. 读卡 (杜邦线环境建议不要频繁开关中断，除非极其不稳)
    FRESULT res = f_read(&mp3File, mp3InBuf + bytesLeft, space, &br);
    if (res != FR_OK) return -1;

    real_time_bytes += br; // <--- 关键：没这行，速度永远是 0
    bytesLeft += br;
    return br;
}


/**
 * 跳过 ID3v2 标签头
 */
static void Skip_ID3_Tag(void) {
    uint8_t header[10];
    UINT br;

    f_lseek(&mp3File, 0);
    f_read(&mp3File, header, 10, &br);

    if (br == 10 && header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        // 计算标签大小 (每个字节低7位有效)
        uint32_t size = ((header[6] & 0x7F) << 21) |
                        ((header[7] & 0x7F) << 14) |
                        ((header[8] & 0x7F) << 7)  |
                         (header[9] & 0x7F);
        f_lseek(&mp3File, size + 10);
    } else {
        f_lseek(&mp3File, 0); // 不是ID3，回到开头
    }

    // 重置流指针
    bytesLeft = 0;
    readPtr = mp3InBuf;
}

/**
 * 解码一帧并写入目标缓冲区
 * target_buf: AudioBuffer 中的某个偏移位置
 */
static int Decode_Frame_To_Buffer(int16_t *target_buf) {
    // 1. 检查数据余量
    if (bytesLeft < 2048) {
        if (Fill_Stream_Buffer() <= 0 && bytesLeft == 0) return -1; // 文件结束
    }

    // 2. 寻找同步字
    int offset = MP3FindSyncWord(readPtr, bytesLeft);
    if (offset < 0) {
        // 没找到，丢弃大部分数据，防止死循环
        if (bytesLeft > 1024) {
            readPtr += 1024; bytesLeft -= 1024;
        } else {
            bytesLeft = 0; // 清空重读
        }
        return -2;
    }

    readPtr += offset;
    bytesLeft -= offset;

    // 3. 解码
    int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, target_buf, 0);

    if (err == ERR_MP3_NONE) {
        MP3GetLastFrameInfo(hMP3Decoder, &g_mp3FrameInfo);

        // 应用音量
        Apply_Volume(target_buf, g_mp3FrameInfo.outputSamps);

        // 更新统计
        playedSamples += g_mp3FrameInfo.outputSamps / 2;

        return 0; // 成功
    } else {
        // 错误处理：跳过少量字节尝试恢复
        if (bytesLeft > 2) { readPtr += 2; bytesLeft -= 2; }
        else bytesLeft = 0;
        return err;
    }
}

/* ================== 外部接口实现 ================== */

void Audio_Init(void) {
    hMP3Decoder = MP3InitDecoder();
    audioStatus = AUDIO_IDLE;
}

FRESULT Audio_Play(const char* filename) {
    Audio_Stop(); // 停止当前播放

    FRESULT res = f_open(&mp3File, filename, FA_READ);
    if (res != FR_OK) {
        audioStatus = AUDIO_ERROR;
        return res;
    }

    // 1. 准备数据
    Skip_ID3_Tag();
    playedSamples = 0;
    memset(AudioBuffer, 0, sizeof(AudioBuffer));
    fillBufferFlag = 0;

    // 2. 预解码探测 (检测采样率)
    // 解码第一帧到缓冲区头部
    if (Decode_Frame_To_Buffer((int16_t*)AudioBuffer) == 0) {
        // 只有在开始时允许调整 I2S 时钟
        Audio_Set_I2S_Freq(g_mp3FrameInfo.samprate);
    }

    // 3. 启动 DMA (循环模式)
    // 长度单位是 uint16_t 的数量
    HAL_I2S_Transmit_DMA(&hi2s2, AudioBuffer, AUDIO_BUF_SIZE / 2);

    audioStatus = AUDIO_PLAYING;
    return FR_OK;
}

void Audio_Process(void) {
    if (audioStatus != AUDIO_PLAYING || fillBufferFlag == 0) return;

    // 1. 尝试给码流缓冲区补货 (使用重构后的函数名)
    Fill_Stream_Buffer();

    // 2. 锁定要填写的半区并立即清除标志
    uint8_t part = fillBufferFlag;
    fillBufferFlag = 0;

    // 3. 计算半区采样点边界 (针对 16KB 缓冲区)
    // 数组总长 8192 个 uint16 (16384字节)，半区是 4096 个位置
    uint32_t half_samples = AUDIO_BUF_SIZE / 4;
    uint32_t start_idx = (part == 1) ? 0 : half_samples;
    uint32_t end_idx   = start_idx + half_samples;
    uint32_t curr_idx  = start_idx;

    // (我是牢理) 核心补丁：死磕式填充循环
    // 只要当前填充位置还没摸到半区的边界，就一直解！
    while (curr_idx < end_idx) {
        // 计算当前半区剩下的空间
        uint32_t space_left = end_idx - curr_idx;

        // 如果剩下的空间不够塞下一帧最大的 PCM (1152*2 = 2304)
        if (space_left < 2304) {
            // (我是牢理) 重点：把剩下的这丁点空间填成静音(0)
            // 彻底消除由于旧数据残留导致的“噼啪”声
            for(uint32_t s = curr_idx; s < end_idx; s++) {
                AudioBuffer[s] = 0;
            }
            break;
        }

        // 调用重构后的解码接口，直接解到 AudioBuffer 对应位置
        int err = Decode_Frame_To_Buffer((int16_t*)&AudioBuffer[curr_idx]);

        if (err == 0) {
            // 成功，指针累加实际解出的采样数
            curr_idx += g_mp3FrameInfo.outputSamps;
        } else if (err == -1) {
            // 文件播放结束
            Audio_Stop();
            audioStatus = AUDIO_FINISHED;
            break;
        } else {
            // 如果这一帧解坏了，跳过 1 个采样点（2字节）继续尝试，防止死循环
            curr_idx += 2;
        }
    }
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
AudioStatus_t Audio_GetStatus(void) { return audioStatus; }
void Audio_SetVolume(uint8_t vol) {
    currentVolume = (vol > 100) ? 100 : vol;
}

uint8_t Audio_GetVolume(void) {
    return currentVolume;
}

uint32_t Audio_GetElapsedSec(void) {
    if (currentSampleRate == 0) return 0;
    return playedSamples / currentSampleRate;
}

void Audio_ResetTimer(void) {
    playedSamples = 0;
}

/* ================== 中断回调 ================== */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) fillBufferFlag = 1; // 填前半部
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s->Instance == SPI2) fillBufferFlag = 2; // 填后半部
}