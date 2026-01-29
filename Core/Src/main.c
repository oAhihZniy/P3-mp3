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
#include <tgmath.h>
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
  // --- 2. 视觉系统启动 ---
  OLED_Init();
  OLED_Clear();
  OLED_ShowString(0, 0, "PERSONA 3 MP3", 1);
  OLED_ShowString(0, 16, "INITIALIZING...", 1);
  OLED_Update();

  // --- 3. 存储系统启动 ---
  if (SD_Init() == 0) {
    OLED_ShowString(0, 32, "SD PHYS: OK", 1);
  } else {
    OLED_ShowString(0, 32, "SD PHYS: FAIL", 1);
    OLED_Update();
    while(1); // 物理层不通，检查杜邦线
  }
  OLED_Update();

  // --- 4. 挂载文件系统 (LFN_UNICODE=0 模式) ---
  res = f_mount(&fs, "0:", 1);
  if (res == FR_OK) {
    OLED_ShowString(0, 48, "FS MOUNT: OK", 1);
  } else {
    char err_msg[32];
    sprintf(err_msg, "MOUNT ERR: %d", res);
    OLED_ShowString(0, 48, err_msg, 1);
    OLED_Update();
    while(1);
  }
  OLED_Update();
  HAL_Delay(500);

  // --- 5. 加载字库文件 ---
  // (我是牢理提醒) 确保SD卡 SYSTEM 目录下有 FONT.fon
  if (OLED_FontInit("0:/SYSTEM/font.bin") == FR_OK) {
    OLED_Clear();
    OLED_ShowString(0, 0, "FONT LOADED", 1);
    // 验证字库：画一个“中”
    OLED_ShowSDString(100, 0, "中");
  } else {
    OLED_Clear();
    OLED_ShowString(0, 0, "FONT MISSING!", 1);
  }
  OLED_Update();

  // --- 6. 音频引擎与播放列表点火 ---
  Audio_Init(); // 初始化Helix解码器
  if (Playlist_Init() == FR_OK && g_playlist.total_count > 0) {
    char info[32];
    sprintf(info, "FOUND %d SONGS", g_playlist.total_count);
    OLED_ShowString(0, 16, info, 1);
    OLED_Update();
    HAL_Delay(1000);

    // (我是牢理) 正式点火：播放第一首 MP3
    Playlist_PlayCurrent();
  } else {
    OLED_ShowString(0, 16, "NO MP3 FOUND", 1);
    OLED_Update();
  }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {

    // (我是牢理) 任务 1: 音频解码 (极高优先级)
    // 这个函数会处理双缓冲填充，必须尽可能频繁地调用
    Audio_Process();

    // (我是牢理) 任务 2: 逻辑控制 (自动切歌)
    Playlist_AutoNext_Task();

    // (我是牢理) 任务 3: 按键交互 (每20ms一次)
    if (HAL_GetTick() - key_timer >= 20) {
      key_timer = HAL_GetTick();
      App_Task_Keyboard();
    }

    // (我是牢理) 任务 4: UI表现 (每30ms一次)
    // 负责平滑滚动歌名和进度条更新
    if (HAL_GetTick() - ui_timer >= 30) {
      ui_timer = HAL_GetTick();
      UI_Refresh_Task(); // 内部包含 OLED_Update()
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
