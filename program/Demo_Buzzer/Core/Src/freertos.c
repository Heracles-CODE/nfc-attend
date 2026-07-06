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
#include "midi.h"
#include "key.h"
#include "uart_drv.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "usart.h"
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
/* 串口驱动实例 (用于 printf 调试输出) */
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
/* USER CODE END Variables */

/* Definitions for buzzerTask */
osThreadId_t buzzerTaskHandle;
const osThreadAttr_t buzzerTask_attributes = {
  .name = "buzzerTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartBuzzerTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* ========== 小星星音乐数据 ========== */
/* 使用 M1(x) = 中音八度四分音符, M2(x) = 中音八度二分音符 */
/* 音符编码: 1=C, 2=D, 3=E, 4=F, 5=G, 6=A, 7=B, 0=休止符 */
static uint32_t twinkle_twinkle[] = {
    M1(1), M1(1), M1(5), M1(5), M1(6), M1(6), M2(5),
    M1(4), M1(4), M1(3), M1(3), M1(2), M1(2), M2(1),
    M1(5), M1(5), M1(4), M1(4), M1(3), M1(3), M2(2),
    M1(5), M1(5), M1(4), M1(4), M1(3), M1(3), M2(2),
    M1(1), M1(1), M1(5), M1(5), M1(6), M1(6), M2(5),
    M1(4), M1(4), M1(3), M1(3), M1(2), M1(2), M2(1),
};

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
  /* creation of buzzerTask */
  buzzerTaskHandle = osThreadNew(StartBuzzerTask, NULL, &buzzerTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartBuzzerTask */
/**
  * @brief  蜂鸣器音乐播放示例任务
  *         - 第一阶段: 开机自动播放5秒小星星
  *         - 第二阶段: 按键播放对应音符提示音
  *           K1=Do  K2=Re  K3=Mi  K4=Fa  K5=Sol  K6=La
  *         - K1长按: 重新播放小星星
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartBuzzerTask */
void StartBuzzerTask(void *argument)
{
  /* USER CODE BEGIN StartBuzzerTask */
  /* 初始化串口驱动并设置为 printf 调试输出口 */
  UartDrv_Init(&g_uart1Drv, &huart1);
  UartDrv_SetDebugPort(&g_uart1Drv);

  /* 初始化按键驱动 */
  Key_Init(keyConfigs, 6);

  /* 初始化 MIDI 驱动 (TIM3_CH1, PB4 - BEEP) */
  MIDI_Init(&htim3, TIM_CHANNEL_1);
  MIDI_SetVolume(50);

  printf("Buzzer Demo Started\r\n");

  /* === 第一阶段: 播放5秒小星星 === */
  printf("Playing Twinkle Twinkle Little Star...\r\n");
  MIDI_SetMusic(twinkle_twinkle, ARRAY_SIZE(twinkle_twinkle), MIDI_DEFAULT_BPM);
  MIDI_SetDuration(5);  /* 播放5秒 */
  MIDI_Play();

  /* 等待音乐播放完成 */
  while (MIDI_GetState() == MIDI_STATE_PLAYING)
  {
    MIDI_Tick();
    osDelay(10);
  }
  printf("Music finished. Press K1~K6 to play notes.\r\n");

  /* === 第二阶段: 按键播放提示音 === */
  for(;;)
  {
    /* 按键扫描 (每 10ms 调用一次) */
    Key_Scan();

    /* K1 短按: Do */
    if (Key_IsShortPressed(KEY_K1) || Key_IsRepeat(KEY_K1))
    {
      MIDI_Beep(1, 200);  /* Do */
      printf("K1: Do\r\n");
    }

    /* K2 短按: Re */
    if (Key_IsShortPressed(KEY_K2) || Key_IsRepeat(KEY_K2))
    {
      MIDI_Beep(2, 200);  /* Re */
      printf("K2: Re\r\n");
    }

    /* K3 短按: Mi */
    if (Key_IsShortPressed(KEY_K3) || Key_IsRepeat(KEY_K3))
    {
      MIDI_Beep(3, 200);  /* Mi */
      printf("K3: Mi\r\n");
    }

    /* K4 短按: Fa */
    if (Key_IsShortPressed(KEY_K4) || Key_IsRepeat(KEY_K4))
    {
      MIDI_Beep(4, 200);  /* Fa */
      printf("K4: Fa\r\n");
    }

    /* K5 短按: Sol */
    if (Key_IsShortPressed(KEY_K5) || Key_IsRepeat(KEY_K5))
    {
      MIDI_Beep(5, 200);  /* Sol */
      printf("K5: Sol\r\n");
    }

    /* K6 短按: La */
    if (Key_IsShortPressed(KEY_K6) || Key_IsRepeat(KEY_K6))
    {
      MIDI_Beep(6, 200);  /* La */
      printf("K6: La\r\n");
    }

    /* MIDI 节拍处理 */
    MIDI_Tick();

    osDelay(KEY_SCAN_INTERVAL_MS);  /* 10ms 扫描周期 */
  }
  /* USER CODE END StartBuzzerTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
