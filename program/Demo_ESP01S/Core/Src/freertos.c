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
#include "esp01s.h"
#include "uart_drv.h"
#include <stdio.h>


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
/* USART6 驱动实例 (用于 ESP01S 通信) */
static UartDrv_t g_uart6Drv;
/* USER CODE END Variables */

/* Definitions for networkTask */
osThreadId_t networkTaskHandle;
const osThreadAttr_t networkTask_attributes = {
  .name = "networkTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* ESP01S 透传数据接收回调 */
static void ESP01S_DataCallback(const uint8_t *pData, uint16_t len, void *pUserCtx);
/* USER CODE END FunctionPrototypes */

void StartNetworkTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  ESP01S 透传数据接收回调
  * @param  pData: 接收到的数据指针
  * @param  len: 数据长度
  * @param  pUserCtx: 用户上下文 (未使用)
  */
static void ESP01S_DataCallback(const uint8_t *pData, uint16_t len, void *pUserCtx)
{
  (void)pUserCtx;
  printf("[SERVER] %.*s\r\n", len, pData);
}

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
  /* creation of networkTask */
  networkTaskHandle = osThreadNew(StartNetworkTask, NULL, &networkTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartNetworkTask */
/**
  * @brief  Function implementing the networkTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartNetworkTask */
void StartNetworkTask(void *argument)
{
  /* USER CODE BEGIN StartNetworkTask */
  int ret;
  uint32_t tickStart;

  /* 初始化 USART1 驱动实例 (用于 printf 调试输出) */
  UartDrv_Init(&g_uart1Drv, &huart1);
  UartDrv_SetDebugPort(&g_uart1Drv);

  /* 初始化 USART6 驱动实例 (用于 ESP01S 通信) */
  UartDrv_Init(&g_uart6Drv, &huart6);

  /* 初始化 ESP01S 模块 (内部注册 UART6 接收回调) */
  ESP01S_Init(&g_uart6Drv);

  /* 注册透传数据接收回调 (收到服务器数据时打印) */
  ESP01S_RegisterDataCb(ESP01S_DataCallback, NULL);

  printf("ESP01S Demo: Connecting WiFi & TCP...\r\n");

  /* 启动 ESP01S 连接流程 (阻塞, 等待 WiFi + TCP 连接完成) */
  ret = ESP01S_Start();

  if (ret == 0)
  {
    printf("ESP01S Demo: Connected! Sending 'hello' every second...\r\n");

    tickStart = HAL_GetTick();

    /* Infinite loop */
    for(;;)
    {
      /* 每隔 1 秒发送 "hello" 字符串到服务器 */
      if (HAL_GetTick() - tickStart >= 1000)
      {
        tickStart = HAL_GetTick();
        ESP01S_SendStr("hello\n");
      }

      osDelay(50);
    }
  }
  else
  {
    printf("ESP01S Demo: Connection failed! ret=%d\r\n", ret);
  }

  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartNetworkTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
