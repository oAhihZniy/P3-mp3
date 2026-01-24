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
FATFS fs;                 // FatFs 文件系统对象
FRESULT res;              // 文件操作结果
uint32_t ui_timer = 0;    // UI 刷新计时器
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  // 1. 初始化 OLED 屏幕
  OLED_Init();
  OLED_Clear();
  OLED_ShowString(0, 0, "P3 MP3 STARTING", 1);
  OLED_Update();

  // 2. 挂载 SD 卡
  res = f_mount(&fs, "0:/", 1);
  if (res != FR_OK) {
    OLED_Clear();
    OLED_ShowString(0, 0, "SD MOUNT ERROR", 1);
    char err_buf[16];
    sprintf(err_buf, "CODE: %d", res);
    OLED_ShowString(0, 16, err_buf, 1);
    OLED_Update();
    while(1); // 挂载失败则停机，检查硬件
  }

  // 3. 初始化字库 (SD卡挂载后才能执行)
  if (OLED_FontInit("0:/FONT.BIN") != FR_OK) {
    // 如果没有字库，虽然能播歌但不能显示中日文
    OLED_ShowString(0, 16, "FONT NOT FOUND", 1);
    OLED_Update();
    HAL_Delay(1000);
  }

  // 4. 初始化音频解码器和播放列表
  Audio_Init();
  if (Playlist_Init() == FR_OK && g_playlist.total_count > 0) {
    // 成功搜到歌曲，开始播放第一首
    Playlist_PlayCurrent();
  } else {
    OLED_Clear();
    OLED_ShowString(0, 0, "NO MP3 FILES!", 1);
    OLED_Update();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // --- 任务 1: 音频解码 (最高优先级) ---
    // 这个函数必须在主循环中以最快速度被轮询
    Audio_Process();

    // --- 任务 2: 自动切歌检查 ---
    // 检查当前歌曲是否自然结束，若是则切换下一首
    Playlist_AutoNext_Task();

    // --- 任务 3: 按键处理 (非阻塞) ---
    // 每 20ms 扫描一次按键，内部已处理消抖
    App_Task_Keyboard();

    // --- 任务 4: UI 界面刷新 (非阻塞) ---
    // 约 33 FPS (30ms 刷新一次)，保证滚动歌名丝滑
    if (HAL_GetTick() - ui_timer >= 30) {
      ui_timer = HAL_GetTick();
      UI_Refresh_Task(); // 内部会调用 OLED_Update()
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
