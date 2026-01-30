#ifndef __SD_CARD_SPI_H
#define __SD_CARD_SPI_H

#include "main.h"

// SD卡命令定义 (R1响应)
#define CMD0   0   // 卡复位
#define CMD1   1   // 查询OCR
#define CMD8   8   // 检查电压 (SD2.0专用)
#define CMD9   9   // 读取CSD
#define CMD10  10  // 读取CID
#define CMD12  12  // 停止传输
#define CMD16  16  // 设置块长度
#define CMD17  17  // 读单块
#define CMD18  18  // 读多块
#define CMD23  23  // 设置多块擦除数量
#define CMD24  24  // 写单块
#define CMD25  25  // 写多块
#define CMD41  41  // 应答应答条件命令 (ACMD)
#define CMD55  55  // 特定应用命令的前导命令
#define CMD58  58  // 读取OCR

// SD卡物理特性定义
#define SD_TYPE_ERR     0x00
#define SD_TYPE_MMC     0x01
#define SD_TYPE_V1      0x02
#define SD_TYPE_V2      0x04
#define SD_TYPE_V2HC    0x06

// 片选信号控制 (PA4)
#define SD_CS_LOW()     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define SD_CS_HIGH()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)
void SD_SPI_SpeedHigh(void);
// 函数声明
uint8_t SD_Init(void);
uint8_t SD_ReadDisk(uint8_t* buf, uint32_t sector, uint8_t cnt);
uint8_t SD_WriteDisk(uint8_t* buf, uint32_t sector, uint8_t cnt);

#endif