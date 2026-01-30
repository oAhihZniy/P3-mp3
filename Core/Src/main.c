/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sd_card_spi.h"
#include "audio_driver.h"
#include "playlist.h"
#include "oled_driver.h"
#include "oled_app.h"
#include "app_task.h"
#include <stdio.h>
#include <string.h>

#include "mp3dec.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
FATFS fs;                 // 全局文件系统对象（必须放全局，防止栈溢出）
FRESULT res;              // FatFs返回值
uint32_t ui_timer = 0;    // UI刷新计时
uint32_t key_timer = 0;   // 按键扫描计时

uint32_t real_time_bytes = 0;
uint32_t last_stat_tick = 0;
int current_bw = 0.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* main.c Bundle up your dreams.mp3*/
#include <stdio.h>

/**
 * (我是牢理) SD卡读取速度专项测试
 * 逻辑：连续读取 1MB 数据，计算耗时
 */
void Test_SD_Read_Speed(void) {
  FIL file;
  UINT br;
  uint8_t temp_buf[4096]; // 每次读 4KB
  uint32_t start_tick, end_tick;
  uint32_t total_read = 0;
  const uint32_t target_size = 1024 * 1024; // 测试 1MB 数据
  char msg[32];

  OLED_Clear();
  OLED_ShowString(0, 0, "TESTING SPEED...", 1);
  OLED_Update();

  // 1. 打开一个现成的 MP3 文件进行测试
  if (f_open(&file, "0:/Bundle up your dreams.mp3", FA_READ) != FR_OK) {
    OLED_ShowString(0, 16, "OPEN FAIL!", 1);
    OLED_Update();
    return;
  }

  // 2. 开始计时
  start_tick = HAL_GetTick();

  while (total_read < target_size) {
    // 尝试读取数据
    FRESULT res = f_read(&file, temp_buf, sizeof(temp_buf), &br);
    if (res != FR_OK || br == 0) break;
    total_read += br;
  }

  end_tick = HAL_GetTick();
  f_close(&file);

  // 3. 计算速度
  uint32_t duration = end_tick - start_tick; // 耗时(ms)
  if (duration == 0) duration = 1; // 防止除以0

  // 速度 (KB/s) = (总字节 / 1024) / (时间 / 1000)
  // 简化为：字节 / ms
  uint32_t speed = total_read / duration;

  // 4. 显示结果
  OLED_Clear();
  OLED_ShowString(0, 0, "READ SPEED TEST", 1);

  sprintf(msg, "Size: %lu KB", total_read / 1024);
  OLED_ShowString(0, 16, msg, 1);

  sprintf(msg, "Time: %lu ms", duration);
  OLED_ShowString(0, 32, msg, 1);

  sprintf(msg, "Speed: %lu KB/s", speed);
  OLED_ShowString(0, 48, msg, 1);

  OLED_Update();

  // 停在这里让我们看清数据
  while(1);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2S2_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();
  OLED_Clear();
  OLED_ShowString(0, 0, "SYSTEM BOOT...", 1);
  OLED_Update();

  // --- 2. 启动存储物理层 ---
  if (SD_Init() == 0) {
    OLED_ShowString(0, 16, "SD HARDWARE: OK", 1);
  } else {
    OLED_ShowString(0, 16, "SD HARDWARE: FAIL", 1);
    OLED_Update();
    while(1); // 物理层挂了，死循环等待检查
  }
  OLED_Update();

  // --- 3. 挂载文件系统 (GBK模式，无 L 前缀) ---
  res = f_mount(&fs, "0:", 1);
  if (res == FR_OK) {
    SD_SPI_SpeedHigh();
    OLED_ShowString(0, 32, "MOUNT: SUCCESS", 1);
  } else {
    char err_buf[32];
    sprintf(err_buf, "MOUNT ERR: %d", res);
    OLED_ShowString(0, 32, err_buf, 1);
    OLED_Update();
    while(1);
  }
  OLED_Update();

  // --- 4. 加载中文字库 ---
  // (我是牢理) 确保你的路径正确，区分大小写
  res = OLED_FontInit("0:/SYSTEM/font.bin");
  if (res == FR_OK) {
    OLED_ShowString(0, 48, "FONT LOADED", 1);
  } else {
    // 没字库也能跑，只是中文不显示，所以不卡死
    OLED_ShowString(0, 48, "FONT MISSING", 1);
  }
  OLED_Update();


  // --- 5. 音频引擎初始化 ---
  Audio_Init();

  // --- 6. 扫描歌曲并播放 ---
  if (Playlist_Init() == FR_OK && g_playlist.total_count > 0) {
    OLED_Clear();
    char info[32];
    sprintf(info, "SONGS: %d", g_playlist.total_count);
    OLED_ShowString(0, 0, info, 1);
    OLED_ShowString(0, 16, "STARTING...", 1);
    OLED_Update();


    // (我是牢理) 点火！播放第一首歌
    Playlist_PlayCurrent();
  } else {
    OLED_Clear();
    OLED_ShowString(0, 0, "NO MP3 FILES!", 1);
    OLED_Update();
  }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    // (我是牢理) 任务 1: 音频核心 (最高优先级)
    // 只要 DMA 发出半传输中断，这里就会立刻把数据从 SD 卡搬到 RAM 并拉伸格式
    Audio_Process();

    // (我是牢理) 任务 2: 播放逻辑
    // 检查是否播完了，播完自动切歌
    Playlist_AutoNext_Task();

    // (我是牢理) 任务 3: 按键交互 (每 20ms 扫描一次)
    if (HAL_GetTick() - key_timer >= 20) {
      key_timer = HAL_GetTick();
      App_Task_Keyboard();
    }

    // (我是牢理) 任务 4: UI 刷新 (每 50ms 一次)
    // 杜邦线环境下，适当降低 UI 刷新率(从30ms降到50ms)，把 SPI 带宽让给音频
    if (HAL_GetTick() - ui_timer >= 30) {
      ui_timer = HAL_GetTick();
      UI_Refresh_Task(); // 滚动歌名、进度条
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 200;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
