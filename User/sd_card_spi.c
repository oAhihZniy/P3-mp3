#include "sd_card_spi.h"
#include "spi.h"

extern SPI_HandleTypeDef hspi1;
volatile uint8_t g_dma_rx_done = 0; // DMA 接收完成标志

// 1. SPI 读写单字节 (基础)
static uint8_t SPI_ReadWriteByte(uint8_t data) {
    uint8_t res;
    HAL_SPI_TransmitReceive(&hspi1, &data, &res, 1, HAL_MAX_DELAY);
    return res;
}

// 2. SPI 切换到高速模式 (初始化完成后调用)
void SD_SPI_SpeedHigh(void) {
    __HAL_SPI_DISABLE(&hspi1);
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; // 100MHz/4 = 25MHz
    HAL_SPI_Init(&hspi1);
    __HAL_SPI_ENABLE(&hspi1);
}

// 3. 等待 SD 卡准备就绪
uint8_t SD_WaitReady(void) {
    uint32_t t = 0;
    // 等待 MISO 变高（0xFF），代表卡闲置
    do {
        if (SPI_ReadWriteByte(0xFF) == 0xFF) return 0;
        t++;
    } while (t < 0x5000); // 400kHz 下这个时间足够了
    return 1;
}

// 4. 发送命令 1.0
// uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
//     uint8_t res;
//     uint8_t retry = 0;
//
//     // 1. 确保释放总线并多给 8 个时钟，让卡完成上一指令
//     SD_CS_HIGH();
//     SPI_ReadWriteByte(0xFF);
//
//     // 2. 选中卡
//     SD_CS_LOW();
//
//     // 3. 等待卡准备好 (MISO 应该为高)
//     if (SD_WaitReady()) {
//         SD_CS_HIGH();
//         return 0xFF;
//     }
//
//     // 4. 发送命令包
//     SPI_ReadWriteByte(cmd | 0x40);
//     SPI_ReadWriteByte(arg >> 24);
//     SPI_ReadWriteByte(arg >> 16);
//     SPI_ReadWriteByte(arg >> 8);
//     SPI_ReadWriteByte(arg);
//     SPI_ReadWriteByte(crc);
//
//     // 5. 特别处理：CMD12 停止指令后需要跳过一个字节
//     if (cmd == CMD12) SPI_ReadWriteByte(0xFF);
//
//     // 6. 等待 R1 响应 (最高位必须为 0)
//     do {
//         res = SPI_ReadWriteByte(0xFF);
//     } while ((res & 0x80) && retry++ < 0XFE);
//
//     // 注意：这里先不要释放 CS，让调用者决定何时释放（因为读数据需要持续拉低 CS）
//     return res;
// }


// 4. 发送命令 1.1 (增加等待深度，提升兼容性)
uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t res;
    uint8_t retry = 0;

    // 1. 关键：先释放 CS 并发送 8 个时钟，强制让卡片退出之前的任何状态
    SD_CS_HIGH();
    SPI_ReadWriteByte(0xFF);

    // 2. 选中卡片
    SD_CS_LOW();

    // 3. 等待卡片就绪 (MISO 应为高)
    if (SD_WaitReady()) {
        SD_CS_HIGH();
        return 0xFF;
    }

    // 4. 发送 6 字节命令包
    SPI_ReadWriteByte(cmd | 0x40);
    SPI_ReadWriteByte(arg >> 24);
    SPI_ReadWriteByte(arg >> 16);
    SPI_ReadWriteByte(arg >> 8);
    SPI_ReadWriteByte(arg);
    SPI_ReadWriteByte(crc);

    // 5. 特别处理：CMD12 停止指令后需要跳过一个字节
    if (cmd == CMD12) SPI_ReadWriteByte(0xFF);

    // 6. 等待 R1 响应 (最高位必须为 0)
    // (我是牢理) 增加等待深度，确保慢速卡能响应
    do {
        res = SPI_ReadWriteByte(0xFF);
    } while ((res & 0x80) && retry++ < 0xFF);

    // 注意：这里我们故意让 CS 保持 LOW，交给 ReadDisk 函数去处理后续数据读取
    return res;
}


// 5. 初始化 SD 卡 (核心步骤)
uint8_t SD_Init(void) {
    uint8_t res, i;
    uint8_t buf[4];

    // A. 强制低速
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    HAL_SPI_Init(&hspi1);

    // B. 必须在 CS 为高电平时发送至少 74 个时钟脉冲
    SD_CS_HIGH();
    for (i = 0; i < 15; i++) SPI_ReadWriteByte(0xFF);

    // C. 发送 CMD0
    res = SD_SendCmd(CMD0, 0, 0x95);
    if (res != 0x01) return 10; // 如果返回 255，说明 MISO 一直是高，卡没反应

    // D. 发送 CMD8
    res = SD_SendCmd(CMD8, 0x1AA, 0x87);
    if (res != 0x01) return 20;// 不是 SD2.0 卡

    for (i = 0; i < 4; i++) buf[i] = SPI_ReadWriteByte(0xFF);

    // E. 激活卡 (ACMD41)
    uint32_t retry = 2000;
    do {
        // 先发送 CMD55，必须返回 0x01
        res = SD_SendCmd(CMD55, 0, 0x01);
        SD_CS_HIGH(); // 释放一下 CS
        SPI_ReadWriteByte(0xFF); // 提供 8 个时钟

        if (res <= 0x01) { // 如果 CMD55 响应正常
            // 再发送 CMD41
            res = SD_SendCmd(CMD41, 0x40000000, 0x01);
            SD_CS_HIGH(); // 释放 CS
            SPI_ReadWriteByte(0xFF);
        }
        HAL_Delay(1);
    } while (res != 0x00 && retry--);

    if (res != 0x00) return 30; // 依然超时返回 30

    // F. 初始化成功后提速
    SD_SPI_SpeedHigh();
    SD_CS_HIGH();
    return 0;
}

// 6. DMA 读取块 (大幅提升 MP3 读取性能)
// 覆写 SPI 的 DMA 完成回调
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        g_dma_rx_done = 1;
    }
}

// uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
//     uint8_t res;
//     if (cnt == 1) {
//         res = SD_SendCmd(CMD17, sector, 0x01); // 读单块
//         if (res == 0) {
//             // 等待起始令牌 0xFE
//             while (SPI_ReadWriteByte(0xFF) != 0xFE);
//
//             // 使用 DMA 接收 512 字节
//             g_dma_rx_done = 0;
//             HAL_SPI_Receive_DMA(&hspi1, buf, 512);
//             while (!g_dma_rx_done); // 等待 DMA 中断触发
//
//             // 接收 2 字节 CRC
//             SPI_ReadWriteByte(0xFF);
//             SPI_ReadWriteByte(0xFF);
//         }
//     }
//     SD_CS_HIGH();
//     return res;
// }
// 下面的可用 1.0
// uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
//     uint8_t res = 0;
//
//     // 如果是 SDHC 卡，使用扇区地址；如果是老卡，需要 sector << 9
//     // 假设你用的是现代 SDHC 卡
//
//     for (uint8_t i = 0; i < cnt; i++) {
//         res = SD_SendCmd(CMD17, sector + i, 0xFF); // 修改为标准的 CRC 0xFF
//         if (res != 0) break;
//
//         // 等待起始令牌 0xFE
//         uint32_t timeout = 0xFFFF;
//         while (SPI_ReadWriteByte(0xFF) != 0xFE && timeout--);
//         if (timeout == 0) { res = 0xFF; break; }
//
//         // 使用 DMA 接收 512 字节
//         g_dma_rx_done = 0;
//         HAL_SPI_Receive_DMA(&hspi1, buf + (i * 512), 512);
//
//         // 增加安全超时，防止硬件故障导致死循环
//         timeout = 0xFFFF;
//         while (!g_dma_rx_done && timeout--);
//
//         // 接收 2 字节 CRC
//         SPI_ReadWriteByte(0xFF);
//         SPI_ReadWriteByte(0xFF);
//     }
//
//     SD_CS_HIGH();
//     SPI_ReadWriteByte(0xFF); // 释放总线
//     return res;
// }

// 下面的上面那个后的 1.1
 // uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
 //     uint8_t res = 0;
 //
 //     for (uint8_t i = 0; i < cnt; i++) {
 //         // 1. 发送读单块命令
 //         res = SD_SendCmd(CMD17, sector + i, 0xFF);
 //         if (res != 0) {
 //             SD_CS_HIGH(); // 确保失败也释放
 //             return 1;
 //         }
 //
 //         // 2. 等待起始令牌 0xFE
 //         uint32_t timeout = 0x1FFFF;
 //         while (SPI_ReadWriteByte(0xFF) != 0xFE && timeout--);
 //         if (timeout == 0) {
 //             SD_CS_HIGH();
 //             return 0xFF;
 //         }
 //
 //         // 3. DMA 接收数据
 //         g_dma_rx_done = 0;
 //         HAL_SPI_Receive_DMA(&hspi1, buf + (i * 512), 512);
 //
 //         // 4. 等待 DMA 完成
 //         timeout = 0x1FFFF;
 //         while (!g_dma_rx_done && timeout--);
 //         if (timeout == 0) {
 //             SD_CS_HIGH();
 //             return 0xFE;
 //         }
 //
 //         // 5. 跳过 2 字节 CRC
 //         SPI_ReadWriteByte(0xFF);
 //         SPI_ReadWriteByte(0xFF);
 //
 //         // 6. (关键修正) 每读完一块，立即释放 CS 并给 8 个时钟缓冲
 //         SD_CS_HIGH();
 //         SPI_ReadWriteByte(0xFF);
 //     }
 //
 //     return 0;
 // }


// 下面的 1.2 版本，弃用 DMA，改为手动循环读取（暂时弃用）
// uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
     // uint8_t res = 0;
     //
     // for (uint8_t i = 0; i < cnt; i++) {
//         // 1. 发送读命令
//         res = SD_SendCmd(CMD17, sector + i, 0xFF);
//         if (res != 0x00) {
//             SD_CS_HIGH();
//             return 0xFF;
//         }
//
//         // 2. 等待起始令牌 0xFE
//         uint32_t timeout = 0xFFFFFF;
//         while (SPI_ReadWriteByte(0xFF) != 0xFE && timeout--);
//         if (timeout == 0) {
//             SD_CS_HIGH();
//             return 0xFE;
//         }
//
//         // 3. (核心修改) 弃用 DMA，改为手动循环读取
//         // (我是牢理) 这样可以 100% 确定是不是物理信号的问题
//         for (uint32_t j = 0; j < 512; j++) {
//             buf[i * 512 + j] = SPI_ReadWriteByte(0xFF);
//         }
//
//         // 4. 跳过 CRC
//         SPI_ReadWriteByte(0xFF);
//         SPI_ReadWriteByte(0xFF);
//
//         // 5. 释放片选
//         SD_CS_HIGH();
//         SPI_ReadWriteByte(0xFF);
//     }
//     return 0;
// }

// 下面的 1.3 版本，修正 DMA 读完后未释放 CS 导致的问题
// uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
//     uint8_t res = 0;
//     for (uint8_t i = 0; i < cnt; i++) {
//         res = SD_SendCmd(CMD17, sector + i, 0xFF);
//         if (res != 0x00) { SD_CS_HIGH(); return 0xFF; }
//
//         uint32_t timeout = 0x1FFFFF;//0xFFFFF;
//         while (SPI_ReadWriteByte(0xFF) != 0xFE && timeout--);
//         if (timeout == 0) { SD_CS_HIGH(); return 0xFE; }
//
//         // (我是牢理) 重新启用 DMA，但确保 CS 逻辑正确
//         g_dma_rx_done = 0;
//         HAL_SPI_Receive_DMA(&hspi1, buf + (i * 512), 512);
//
//         timeout = 0x1FFFFF;//0xFFFFF;
//         while (!g_dma_rx_done && timeout--);
//         if (timeout == 0) { SD_CS_HIGH(); return 0xFD; }
//
//         SPI_ReadWriteByte(0xFF); // CRC
//         SPI_ReadWriteByte(0xFF);
//
//         SD_CS_HIGH();           // (我是牢理) 关键：每块读完必须释放
//         SPI_ReadWriteByte(0xFF);
//     }
//     return 0;
// }

// 下面的 1.4 版本，增加超时保护，确保不会死循环 !!!标记
// uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
//     uint8_t res = 0;
//     for (uint8_t i = 0; i < cnt; i++) {
//         // 1. 每读一个新扇区，都必须发送一次命令
//         res = SD_SendCmd(CMD17, sector + i, 0xFF);
//         if (res != 0x00) {
//             SD_CS_HIGH();
//             return 1;
//         }
//
//         // 2. 等待起始令牌 0xFE，超时给足 (我是牢理建议给到 0xFFFFF)
//         uint32_t timeout = 0xFFFFF;
//         while (SPI_ReadWriteByte(0xFF) != 0xFE && timeout--);
//         if (timeout == 0) {
//             SD_CS_HIGH();
//             return 1;
//         }
//
//         // 3. DMA 接收 512 字节
//         g_dma_rx_done = 0;
//         // HAL_SPI_Receive_DMA(&hspi1, buf + (i * 512), 512);
//         HAL_SPI_Receive(&hspi1, buf + (i * 512), 512, 100);
//         // 4. 等待 DMA 完成，超时同样给足
//         timeout = 0xFFFFF;
//         while (!g_dma_rx_done && timeout--);
//         if (timeout == 0) {
//             SD_CS_HIGH();
//             return 1;
//         }
//
//         // 5. 丢弃 2 字节 CRC
//         SPI_ReadWriteByte(0xFF);
//         SPI_ReadWriteByte(0xFF);
//
//         // 6. (关键！) 每读完一块，必须立刻释放 CS 并给 8 个时钟缓冲
//         // 这样 SD 卡才能准备好下一次寻址
//         SD_CS_HIGH();
//         SPI_ReadWriteByte(0xFF);
//     }
//     return 0;
// }

// 下面的 2.0 版本，彻底弃用 DMA，改为阻塞式读取，提升稳定性（这个可以正常读取字库）
// uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
//     uint8_t res = 0;
//
//     // (我是牢理) 增加逻辑：如果读取失败，最多重试 3 次
//     for (uint8_t i = 0; i < cnt; i++) {
//         uint8_t retry = 3;
//         while (retry--) {
//             res = SD_SendCmd(CMD17, sector + i, 0xFF);
//             if (res == 0x00) {
//                 // 等待起始令牌 0xFE
//                 uint32_t timeout = 0xFFFFF;
//                 while (SPI_ReadWriteByte(0xFF) != 0xFE && timeout--);
//
//                 if (timeout > 0) {
//                     // (我是牢理) 这里先用阻塞模式测试稳定性
//                     for (uint32_t j = 0; j < 512; j++) {
//                         buf[i * 512 + j] = SPI_ReadWriteByte(0xFF);
//                     }
//                     // 跳过 CRC
//                     SPI_ReadWriteByte(0xFF);
//                     SPI_ReadWriteByte(0xFF);
//
//                     res = 0; // 读取成功
//                     break;   // 跳出重试循环
//                 }
//             }
//             // 如果这一轮失败了，释放总线，稍后再试
//             SD_CS_HIGH();
//             SPI_ReadWriteByte(0xFF);
//             HAL_Delay(1);
//         }
//
//         // (我是牢理) 关键：每一块读完都要彻底释放总线
//         SD_CS_HIGH();
//         SPI_ReadWriteByte(0xFF);
//
//         if (res != 0) return 1; // 彻底失败
//     }
//     return 0;
// }

// 下面的 2.1 版本，恢复 DMA，但保持超时保护和释放总线逻辑
uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
    uint8_t res = 0;

    for (uint8_t i = 0; i < cnt; i++) {
        // 1. 发送读取命令
        res = SD_SendCmd(CMD17, sector + i, 0xFF);
        if (res != 0x00) {
            SD_CS_HIGH();
            return 1;
        }

        // 2. 等待起始令牌 0xFE (稍微给足时间)
        uint32_t timeout = 0x1FFFF;
        while (SPI_ReadWriteByte(0xFF) != 0xFE && timeout--);
        if (timeout == 0) {
            SD_CS_HIGH();
            return 0xFE;
        }

        // 3. 【核心】使用 DMA 接收这一块 (512字节)
        g_dma_rx_done = 0;
        if (HAL_SPI_Receive_DMA(&hspi1, buf + (i * 512), 512) != HAL_OK) {
            SD_CS_HIGH();
            return 1;
        }

        // 4. 等待这一块传输完成 (我是牢理建议增加超时保护)
        timeout = 0x1FFFF;
        while (!g_dma_rx_done && timeout--);
        if (timeout == 0) {
            SD_CS_HIGH();
            return 0xFD;
        }

        // 5. 丢弃 CRC (2字节)
        SPI_ReadWriteByte(0xFF);
        SPI_ReadWriteByte(0xFF);

        // 6. 【关键成功经验】读完每块后，释放片选并提供 8 个空时钟
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);
    }

    return 0;
}

// 1. 等待 SD 卡写完成 (忙检测)
// SD 卡在写入数据时会将 MISO 拉低，直到内部操作完成
static uint8_t SD_WaitWriteDone(void) {
    uint32_t timeout = 0xFFFFF;
    while (SPI_ReadWriteByte(0xFF) != 0xFF) {
        if (timeout-- == 0) return 1; // 超时
    }
    return 0;
}

// 2. 写磁盘函数
uint8_t SD_WriteDisk(uint8_t* buf, uint32_t sector, uint8_t cnt) {
    uint8_t res = 0;

    for (uint8_t i = 0; i < cnt; i++) {
        // 发送写单块命令
        res = SD_SendCmd(CMD24, sector + i, 0xFF);
        if (res != 0) break;

        SD_CS_LOW();
        // A. 发送起始令牌 0xFE
        SPI_ReadWriteByte(0xFF); // 缓冲
        SPI_ReadWriteByte(0xFE);

        // B. 写入 512 字节数据
        // 因为写数据通常不频繁且量小，直接轮询写入即可，不需要复杂的 DMA
        for (uint16_t j = 0; j < 512; j++) {
            SPI_ReadWriteByte(buf[j + (i * 512)]);
        }

        // C. 发送 2 字节伪 CRC
        SPI_ReadWriteByte(0xFF);
        SPI_ReadWriteByte(0xFF);

        // D. 读取响应
        // 响应格式：xxx00101 (0x05) 代表数据已被接受
        uint8_t response = SPI_ReadWriteByte(0xFF);
        if ((response & 0x1F) != 0x05) {
            res = 1;
            SD_CS_HIGH();
            break;
        }

        // E. 等待卡内部写完成
        if (SD_WaitWriteDone() != 0) {
            res = 2;
            SD_CS_HIGH();
            break;
        }
        SD_CS_HIGH();
    }

    SPI_ReadWriteByte(0xFF); // 额外时钟
    return res;
}