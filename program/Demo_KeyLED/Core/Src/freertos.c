/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications - 按键 & LED 示例
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
#include "usart.h"
#include "key.h"
#include "led.h"
#include "uart_drv.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 按键索引定义 */
#define KEY_K1  0
#define KEY_K2  1
#define KEY_K3  2
#define KEY_K4  3
#define KEY_K5  4
#define KEY_K6  5
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* 串口驱动实例(用于printf调试输出) */
static UartDrv_t g_uart1Drv;

/* 按键配置: K1~K4 上拉低有效, K5~K6 下拉高有效 */
static const Key_Config_t keyConfigs[6] = {
    { K1_GPIO_Port, K1_Pin, KEY_ACTIVE_LOW  },  /* K1 */
    { K2_GPIO_Port, K2_Pin, KEY_ACTIVE_LOW  },  /* K2 */
    { K3_GPIO_Port, K3_Pin, KEY_ACTIVE_LOW  },  /* K3 */
    { K4_GPIO_Port, K4_Pin, KEY_ACTIVE_LOW  },  /* K4 */
    { K5_GPIO_Port, K5_Pin, KEY_ACTIVE_HIGH },  /* K5 */
    { K6_GPIO_Port, K6_Pin, KEY_ACTIVE_HIGH },  /* K6 */
};

/* LED 配置: L1~L7 低电平点亮 (灌电流) */
static const Led_Config_t ledConfigs[7] = {
    { L1_GPIO_Port, L1_Pin, LED_ON_LOW },  /* L1 */
    { L2_GPIO_Port, L2_Pin, LED_ON_LOW },  /* L2 */
    { L3_GPIO_Port, L3_Pin, LED_ON_LOW },  /* L3 */
    { L4_GPIO_Port, L4_Pin, LED_ON_LOW },  /* L4 */
    { L5_GPIO_Port, L5_Pin, LED_ON_LOW },  /* L5 */
    { L6_GPIO_Port, L6_Pin, LED_ON_LOW },  /* L6 */
    { L7_GPIO_Port, L7_Pin, LED_ON_LOW },  /* L7 */
};
/* USER CODE END Variables */

/* Definitions for keyLedTask */
osThreadId_t keyLedTaskHandle;
const osThreadAttr_t keyLedTask_attributes = {
  .name = "keyLedTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartKeyLedTask(void *argument);

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
  /* creation of keyLedTask */
  keyLedTaskHandle = osThreadNew(StartKeyLedTask, NULL, &keyLedTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartKeyLedTask */
/**
  * @brief  按键 & LED 示例任务
  *         - K1: 点亮所有 LED
  *         - K4: 熄灭所有 LED
  *         - K2: 亮灯个数加 1 (支持连按)
  *         - K3: 亮灯个数减 1 (支持连按)
  *         - K5: 翻转 L1
  *         - K6: 翻转 L2
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartKeyLedTask */
void StartKeyLedTask(void *argument)
{
  /* USER CODE BEGIN StartKeyLedTask */
  uint8_t ledCount = 0;   /* 亮灯个数 (0~7) */
  uint8_t ledMask = 0;    /* 当前 LED 状态掩码 */

  /* 初始化按键驱动 */
  Key_Init(keyConfigs, 6);

  /* 初始化 LED 驱动 (初始化后全灭) */
  LED_Init(ledConfigs, 7);
  LED_SetLeds(0x00);

  /* 初始化串口驱动并设置printf调试输出口 */
  UartDrv_Init(&g_uart1Drv, &huart1);
  UartDrv_SetDebugPort(&g_uart1Drv);

  printf("Key & LED Demo Started\r\n");

  /* Infinite loop */
  for(;;)
  {
    /* 按键扫描 (每 10ms 调用一次) */
    Key_Scan();

    /* ---- K1: 点亮所有 LED ---- */
    if (Key_IsShortPressed(KEY_K1))
    {
      ledMask = 0x7F;  /* L1~L7 全亮 */
      LED_SetLeds(ledMask);
      ledCount = 7;
      printf("K1: All LEDs ON\r\n");
    }

    /* ---- K4: 熄灭所有 LED ---- */
    if (Key_IsShortPressed(KEY_K4))
    {
      ledMask = 0x00;
      LED_SetLeds(ledMask);
      ledCount = 0;
      printf("K4: All LEDs OFF\r\n");
    }

    /* ---- K3: 亮灯个数加 1 (短按+长按连发) ---- */
    if (Key_IsShortPressed(KEY_K3) || Key_IsRepeat(KEY_K3))
    {
      if (ledCount < 7)
      {
        ledCount++;
        ledMask = (1 << ledCount) - 1;  /* 前 N 个 LED 亮 */
        LED_SetLeds(ledMask);
        printf("K3: LED count=%d (mask=0x%02X)\r\n", ledCount, ledMask);
      }
    }

    /* ---- K2: 亮灯个数减 1 (短按+长按连发) ---- */
    if (Key_IsShortPressed(KEY_K2) || Key_IsRepeat(KEY_K2))
    {
      if (ledCount > 0)
      {
        ledCount--;
        ledMask = (ledCount == 0) ? 0 : ((1 << ledCount) - 1);
        LED_SetLeds(ledMask);
        printf("K2: LED count=%d (mask=0x%02X)\r\n", ledCount, ledMask);
      }
    }

    /* ---- K5: 翻转 L1 ---- */
    if (Key_IsShortPressed(KEY_K5) || Key_IsRepeat(KEY_K5))
    {
      LED_Toggle(0);  /* L1 */
      ledMask = LED_GetLeds();
      printf("K5: Toggle L1 (mask=0x%02X)\r\n", ledMask);
    }

    /* ---- K6: 翻转 L2 ---- */
    if (Key_IsShortPressed(KEY_K6) || Key_IsRepeat(KEY_K6))
    {
      LED_Toggle(1);  /* L2 */
      ledMask = LED_GetLeds();
      printf("K6: Toggle L2 (mask=0x%02X)\r\n", ledMask);
    }

    osDelay(KEY_SCAN_INTERVAL_MS);  /* 10ms 扫描周期 */
  }
  /* USER CODE END StartKeyLedTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
