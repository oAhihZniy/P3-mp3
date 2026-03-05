# P3softmp3 - STM32 MP3 Player

一个基于 STM32F411 的 MP3 播放器项目，支持从 SD 卡读取并播放 MP3 文件，配备 OLED 显示屏和按键控制。

## 项目概述

P3softmp3 是一个功能完整的嵌入式 MP3 播放器解决方案，运行在 STM32F411 微控制器上。该项目使用 Helix MP3 解码器进行音频解码，支持实时播放控制、播放列表管理和中文字符显示。

## 硬件平台

- **微控制器**：STM32F411 (100MHz)
- **存储**：SD 卡 (SPI 接口)
- **显示**：OLED 屏幕 (I2C 接口，0.96寸 128x64)
- **音频输出**：I2S 接口
- **输入控制**：5 个按键 + 耳机插拔检测

## 功能特性

### 核心功能
-  MP3 文件播放（支持各种采样率和比特率）
-  播放列表管理（最多 255 首歌曲）
-  自动连续播放
-  实时音量调节（0-100）

## 按键定义

| 按键 | GPIO 引脚 | 功能 |
|------|-----------|------|
| Play/Pause | PA0 | 播放/暂停切换 |
| Volume + | PA1 | 音量增加 |
| Volume - | PA2 | 音量减小 |
| Previous | PA3 | 上一曲 |
| Next | PB0 | 下一曲 |
| HP Detect | PB1 | 耳机插拔检测 |

## 软件架构

### 主要模块

#### 1. 音频引擎 (`audio_driver.c/h`)
- Helix MP3 解码器集成
- I2S DMA 音频输出
- 音量控制和状态管理

#### 2. SD 卡驱动 (`sd_card_spi.c/h`)
- SPI 接口通信
- FatFS 文件系统集成
- 动态速度切换（硬件检测后提速）

#### 3. 播放列表管理 (`playlist.c/h`)
- 自动扫描 SD 卡中的 MP3 文件
- 支持循环播放
- 文件名索引管理

#### 4. OLED 显示 (`oled_driver.c/h`, `oled_app.c/h`)
- SSD1306 驱动
- 中文字库加载 (`SYSTEM/font.bin`)
- 实时 UI 刷新（进度条、状态显示）

#### 5. 应用任务 (`app_task.c/h`)
- 按键扫描（20ms 周期）
- UI 刷新（40ms 周期）
- 自动切歌检查

### 内存配置

```c
// 音频输出缓冲区（12 帧立体声数据）
#define AUDIO_BUFFER_COUNT  2304*6  // ~27.6KB

// MP3 输入缓冲区
#define MP3_IN_BUF_SIZE     8192   // 8KB

// 播放列表
#define MAX_SONGS 255
#define MAX_FILENAME_LEN 255
```

## 编译说明

### 构建工具
- CMake 3.22+
- ARM GCC 工具链
- STM32CubeMX


### SD 卡文件结构

```
SD Card Root
├── *.mp3              # MP3 音频文件（任意目录）
└── SYSTEM/
    └── font.bin       # 字库文件（全字库Unicode可选，无则只显示英文）
```



## 依赖库

- STM32 HAL 库
- FatFS (R0.12c)
- Helix MP3 Decoder (RealNetworks 开源)
- STM32CubeMX 生成代码

## 系统配置

- 系统时钟：100MHz (HSE + PLL)
- CPU 频率：100MHz
- AHB：100MHz
- APB1：50MHz
- APB2：100MHz

## 许可证

本项目采用 MIT 许可证。包含的第三方库遵循各自的许可证。

## 作者

P3soft Development Team

## 更新日志

### Version 1.0
- 初始版本发布
- 完整的 MP3 播放功能
- OLED 显示支持
- 播放列表管理
- 按键控制

- 确保 `SYSTEM/font.bin` 字库文件存在
- 检查字库文件格式（GBK 编码）
- 文件名使用 GBK 编码


