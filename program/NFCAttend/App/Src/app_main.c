/**
 * @file    app_main.c
 * @brief   在 main 调用的 App 初始化入口
 */
#include "main.h"
#include "bsp_rtc.h"
#include "w25qxx.h"
#include "uart_drv.h"
#include "rc522.h"
#include "led.h"
#include "midi.h"
#include "app_tasks.h"
#include "display.h"
#include "nfc_app.h"
#include "storage.h"
#include "key_proc.h"
#include "gui.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim3;  /* BEEP - TIM3_CH1 */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6;
extern RTC_HandleTypeDef hrtc;     /* RTC 句柄 */

static UartDrv_t g_uart1Drv;

/* 禁用 RTC Alarm A 中断,防止 backup domain 掉电重启后 Alarm 触发复位 */
static void disable_rtc_alarm(void)
{
    __HAL_RTC_ALARM_DISABLE_IT(&hrtc, RTC_IT_ALRA);
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
}

/* NFC 事件回调(非 static,供 app_tasks.c 中的 nfcTask 引用) */
void on_nfc_event(NFC_Event_t evt, const uint8_t uid[4], const CardInfo_t *info)
{
    switch (evt) {
    case NFC_EVT_CARD_VALID:
        LED_SetLeds(0);
        LED_On(0);  /* L1 = 成功 */
        MIDI_Beep(1, 100);  /* C5 */
        if (uid && info) DISP_ShowCardResult((uint8_t *)uid, info, 1);
        printf("[NFC] OK UID=%02X%02X%02X%02X SID=%05u T=%u\n",
               uid[0], uid[1], uid[2], uid[3],
               (unsigned)info->sid, (unsigned)info->type);
        break;

    case NFC_EVT_CARD_INVALID:
        LED_SetLeds(0);
        LED_On(1);  /* L2 = 失败 */
        MIDI_Beep(6, 200);  /* A4 */
        if (uid) DISP_ShowCardResult((uint8_t *)uid, NULL, 0);
        printf("[NFC] ERR UID=%02X%02X%02X%02X\n",
               uid[0], uid[1], uid[2], uid[3]);
        break;

    case NFC_EVT_CARD_LEAVE:
        LED_SetLeds(0);
        DISP_ClearCardResult();
        printf("[NFC] LEAVE\n");
        break;

    default: break;
    }
}

/* 启动时序:由 main 在 MX_* 之后、osKernelStart 之前调用
 *
 * 原则:只做最小初始化,所有可能阻塞/依赖调度器的硬件操作
 * 全部推迟到各自任务的第一次执行。
 */
void App_BeforeScheduler(void)
{
    /* 1) 串口 printf */
    UartDrv_Init(&g_uart1Drv, &huart1);
    UartDrv_SetDebugPort(&g_uart1Drv);
    printf("\r\n=== NFCAttend Phase1 boot ===\r\n");

    /* 2) RTC 初始化后立即禁用 Alarm A,防止断电重启后 Alarm 触发复位 */
    disable_rtc_alarm();

    /* 3) RTC 首次上电设置时间 */
    if (BSP_RTC_IsFirstPowerOn()) {
        BSP_RTC_DateTime_t dt = { .year = 2026, .month = 1, .day = 1,
                                  .hour = 0, .minute = 0, .second = 0, .weekday = 4 };
        BSP_RTC_SetDateTime(&dt);
        BSP_RTC_MarkInitialized();
        printf("[RTC] first power, set 2026-01-01 00:00:00\r\n");
    } else {
        BSP_RTC_DateTime_t dt;
        BSP_RTC_GetDateTime(&dt);
        printf("[RTC] %04u-%02u-%02u %02u:%02u:%02u\r\n",
               dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    }

    /* 4) W25Qxx - 只读 ID 做健康检查 */
    W25QXX_Init();
    uint16_t flash_id = W25QXX_ReadID();
    printf("[W25Q] ID=0x%04X\r\n", flash_id);

    /* 4) LED + 蜂鸣器初始化(纯 GPIO) */
    LED_SetLeds(0);
    MIDI_Init(&htim3, TIM_CHANNEL_1);
    printf("[APP] GPIO init done\r\n");

    /* 5) 按键驱动(纯 GPIO 轮询) */
    KeyProc_Init();
    printf("[APP] Key init done\r\n");

    /* 6) NFC 初始化推迟到 nfcTask,这里只打印提示 */
    printf("[APP] NFC init deferred to nfcTask\r\n");

    printf("[APP] all pre-scheduler init done, starting scheduler...\r\n");
}

/* guiTask 第一次执行时调用(含 I2C) */
void App_GuiTaskInit(void)
{
    /* OLED 含 I2C 通信,必须在调度器启动后调用 */
    DISP_Init();
    printf("[GUI] OLED init done\r\n");
}