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
#include "protocol.h"
#include "led.h"
#include "midi.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>

extern UART_HandleTypeDef huart1;  /* 调试/命令串口 */

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

/* uartTask: 接收 PC 命令(USART1),喂入协议解析器 */
void StartTaskUart(void *argument)
{
    PROTOCOL_Init();
    printf("[UART] protocol ready, waiting for PC commands...\r\n");

    for (;;) {
        /* 轮询 USART1 RX,逐字节读入协议解析器 */
        while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
            uint8_t ch = (uint8_t)(huart1.Instance->DR & 0xFF);
            PROTOCOL_FeedByte(ch);
        }
        osDelay(10);  /* 10ms 轮询周期 */
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