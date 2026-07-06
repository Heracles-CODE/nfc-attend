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
#include "ssd1306.h"
#include "gui.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "rtc.h"
#include "bsp_rtc.h"
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

/* USER CODE END Variables */

/* Definitions for oledTask */
osThreadId_t oledTaskHandle;
const osThreadAttr_t oledTask_attributes = {
  .name = "oledTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartOledTask(void *argument);

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
  /* creation of oledTask */
  oledTaskHandle = osThreadNew(StartOledTask, NULL, &oledTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartOledTask */
/**
  * @brief  Function implementing the oledTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartOledTask */
void StartOledTask(void *argument)
{
  /* USER CODE BEGIN StartOledTask */
  BSP_RTC_DateTime_t dt;
  char buf[24];
  static const char *week_str[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

  /* 初始化 OLED 显示屏和 GUI */
  GUI_Init();
  GUI_SetFont(&GUI_Font8_ASCII);

  /* 首次上电时设置默认时间, 验证日期时间设置功能 */
  if (BSP_RTC_IsFirstPowerOn())
  {
    BSP_RTC_SetDateTime(&(BSP_RTC_DateTime_t){
        .year = 2026, .month = 6, .day = 19,
        .hour = 12, .minute = 0, .second = 0
    });
    BSP_RTC_MarkInitialized();
  }

  /* Infinite loop */
  for(;;)
  {
    /* 读取 RTC 日期时间, 同时更新全局变量 g_rtc_datetime 供其他任务使用 */
    BSP_RTC_GetDateTime(&dt);

    /* 清屏并显示 */
    GUI_Clear();

    /* 显示日期: YYYY-MM-DD Week */
    sprintf(buf, "%04d-%02d-%02d %s", dt.year, dt.month, dt.day, week_str[dt.weekday]);
    GUI_DispStringAt("Date:", 0, 0);
    GUI_DispStringAt(buf, 0, 10);

    /* 显示时间: HH:MM:SS */
    sprintf(buf, "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
    GUI_DispStringAt("Time:", 0, 24);
    GUI_DispStringAt(buf, 0, 34);

    /* 刷新 OLED 显示 */
    SSD1306_UpdateScreen();

    /* 每隔 1 秒更新一次 */
    osDelay(1000);
  }
  /* USER CODE END StartOledTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
