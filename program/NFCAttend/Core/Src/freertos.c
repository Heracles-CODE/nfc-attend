/**
 * @file    freertos.c
 * @brief   NFCAttend 第一阶段:6 任务调度(default/gui/key/uart/other/nfc)
 *
 *  任务入口都在 App/Src/app_tasks.c 实现,这里只负责 osThreadNew 绑定。
 *  与 ioc 中 FREERTOS.Tasks01 的命名/优先级严格保持一致。
 */
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "app_tasks.h"

/* 6 个任务句柄与属性 */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t guiTaskHandle;
const osThreadAttr_t guiTask_attributes = {
  .name = "guiTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t keyTaskHandle;
const osThreadAttr_t keyTask_attributes = {
  .name = "keyTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t uartTaskHandle;
const osThreadAttr_t uartTask_attributes = {
  .name = "uartTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t otherTaskHandle;
const osThreadAttr_t otherTask_attributes = {
  .name = "otherTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t nfcTaskHandle;
const osThreadAttr_t nfcTask_attributes = {
  .name = "nfcTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

void MX_FREERTOS_Init(void)
{
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  guiTaskHandle     = osThreadNew(StartTaskGui,     NULL, &guiTask_attributes);
  keyTaskHandle     = osThreadNew(StartTaskKey,     NULL, &keyTask_attributes);
  uartTaskHandle    = osThreadNew(StartTaskUart,    NULL, &uartTask_attributes);
  otherTaskHandle   = osThreadNew(StartTaskOther,   NULL, &otherTask_attributes);
  nfcTaskHandle     = osThreadNew(StartTaskNFC,     NULL, &nfcTask_attributes);
}