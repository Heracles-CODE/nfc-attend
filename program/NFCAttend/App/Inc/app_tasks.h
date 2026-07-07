/**
 * @file    app_tasks.h
 * @brief   NFCAttend 第一阶段任务入口声明
 */
#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include <stdint.h>

/* 启动初始化(由 main 调用,位于 osKernelStart 之前) */
void App_BeforeScheduler(void);

/* 各任务第一次执行时调用一次,用于延迟初始化(含中断/调度的硬件) */
void App_GuiTaskInit(void);   /* guiTask 首次调用 DISP_Init() */

/* CMSIS-RTOS v2 任务 ID */
typedef void *osThreadId_t;

/* 6 个任务入口(对应 ioc FREERTOS.Tasks01) */
void StartDefaultTask(void *argument);  /* 默认任务(系统监控) */
void StartTaskGui(void *argument);      /* OLED 显示 */
void StartTaskKey(void *argument);      /* 按键扫描/分发 */
void StartTaskUart(void *argument);     /* 调试串口/命令 */
void StartTaskOther(void *argument);    /* 预留(蜂鸣器/W25Q 配置) */
void StartTaskNFC(void *argument);      /* RC522 寻卡 */

/* 任务优先级常量(参照 ioc 中 osPriority_t 的 8 档) */
#define NFC_TASK_PRIO     8
#define GUI_TASK_PRIO     8
#define OTHER_TASK_PRIO   9
#define KEY_TASK_PRIO    24
#define UART_TASK_PRIO   24
#define DEFAULT_TASK_PRIO 24

#endif /* __APP_TASKS_H */