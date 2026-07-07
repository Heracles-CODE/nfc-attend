/**
 * @file    app_tasks.c
 * @brief   6 个任务实体实现(由 freertos.c 调用)
 *
 *   defaultTask  -> 系统心跳/看门狗预留
 *   guiTask      -> OLED 50ms 周期刷新
 *   keyTask      -> 按键 10ms 扫描
 *   uartTask     -> 调试串口(本期仅占位,后续接 ESP01S 命令解析)
 *   otherTask    -> 预留(蜂鸣器状态机/LED 控制)
 *   nfcTask      -> RC522 100ms 周期寻卡
 */
#include "app_tasks.h"
#include "cmsis_os.h"
#include "display.h"
#include "key_proc.h"
#include "nfc_app.h"
#include "led.h"
#include "midi.h"
#include "main.h"
#include <stdio.h>

/* defaultTask:占位 + 周期性 printf */
void StartDefaultTask(void *argument)
{
    uint32_t tick = 0;
    for (;;) {
        if (++tick % 50 == 0) {   /* 每 5s */
            printf("[HB] tick=%lus heap=%u\r\n",
                   (unsigned long)(HAL_GetTick() / 1000),
                   (unsigned)configTOTAL_HEAP_SIZE);
        }
        osDelay(100);
    }
}

/* guiTask:OLED 刷新 + 首次初始化(含 I2C) */
void StartTaskGui(void *argument)
{
    /* 延迟初始化:OLED I2C 必须在调度器启动后才能安全调用 */
    App_GuiTaskInit();

    for (;;) {
        DISP_Update();
        osDelay(50);
    }
}

/* keyTask:10ms 扫描 */
void StartTaskKey(void *argument)
{
    for (;;) {
        KeyProc_Scan10ms();
        osDelay(10);
    }
}

/* uartTask:占位(后续接 ESP01S 命令解析) */
void StartTaskUart(void *argument)
{
    for (;;) {
        osDelay(100);
    }
}

/* otherTask:预留,蜂鸣器状态机 */
void StartTaskOther(void *argument)
{
    for (;;) {
        MIDI_Tick();
        osDelay(20);
    }
}

/* NFC 事件回调(定义在 app_main.c,这里 extern 引用) */
extern void on_nfc_event(NFC_Event_t evt, const uint8_t uid[4], const CardInfo_t *info);

/* nfcTask:100ms 寻卡;首次执行做 NFC 初始化 */
void StartTaskNFC(void *argument)
{
    printf("[NFC] init start (in task)\r\n");
    NFC_App_Init(on_nfc_event);
    printf("[NFC] init done\r\n");

    for (;;) {
        NFC_App_Update();
        osDelay(100);
    }
}