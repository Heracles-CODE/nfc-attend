/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "rc522.h"
#include "uart_drv.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
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
/* USER CODE BEGIN Variables */
/* USART1 驱动实例 (用于 printf 调试输出) */
static UartDrv_t g_uart1Drv;
/* USER CODE END Variables */

/* Definitions for nfcTask */
osThreadId_t nfcTaskHandle;
const osThreadAttr_t nfcTask_attributes = {
  .name = "nfcTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartNfcTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of nfcTask */
  nfcTaskHandle = osThreadNew(StartNfcTask, NULL, &nfcTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartNfcTask */
/**
  * @brief  Function implementing the nfcTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartNfcTask */
void StartNfcTask(void *argument)
{
  /* USER CODE BEGIN StartNfcTask */
  char status;
  uint8_t cardID[4];
  uint8_t allData[64][16];  /* 64 块, 每块 16 字节 */
  int cardPresent = 0;

  /* 初始化 USART1 驱动实例 (用于 printf 调试输出) */
  UartDrv_Init(&g_uart1Drv, &huart1);
  UartDrv_SetDebugPort(&g_uart1Drv);

  /* 初始化 RC522 平台 (软件 SPI + GPIO) */
  RC522_Platform_Init();
  RC522_ConfigISOType('A');

  printf("RC522 NFC Reader Demo Started\r\n");

  /* Infinite loop */
  for(;;)
  {
    /* 寻卡: 检测是否有 IC 卡靠近 */
    status = RC522_ScanCard(cardID);

    if (status == RC522_OK)
    {
      if (!cardPresent)
      {
        cardPresent = 1;
        printf("\r\n=== Card Detected! UID: %02X-%02X-%02X-%02X ===\r\n",
                cardID[0], cardID[1], cardID[2], cardID[3]);

        /* 读取所有扇区数据 */
        status = RC522_ReadAllSectors(allData);

        if (status == RC522_OK)
        {
          /* 打印所有扇区数据 */
          for (int sec = 0; sec < 16; sec++)
          {
            printf("\r\n--- Sector %d ---\r\n", sec);

            for (int blk = 0; blk < 4; blk++)
            {
              int blockIdx = sec * 4 + blk;
              printf("  Block %2d (0x%02X): ", blockIdx, blockIdx);

              for (int i = 0; i < 16; i++)
              {
                printf("%02X ", allData[blockIdx][i]);
              }
              printf("\r\n");
            }
          }
          printf("\r\n=== All sectors read complete ===\r\n");
        }
        else
        {
          printf("Read all sectors failed!\r\n");
          /* 确保卡片回到 HALT 状态，以便后续重新检测 */
          RC522_Halt();
        }
      }
    }
    else
    {
      if (cardPresent)
      {
        cardPresent = 0;
        printf("Card removed.\r\n");
      }
    }

    osDelay(500);  /* 每 500ms 检测一次 */
  }
  /* USER CODE END StartNfcTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
