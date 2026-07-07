/**
 * @file    key_proc.c
 * @brief   按键事件分发
 *
 *  状态机:
 *    时钟       K6 长按 → 时间设置
 *               K6 短按 → 强刷时钟
 *    时间设置   K2/K3 → 当前光标字段 +/-
 *               K6 短按 → 保存并退出
 */
#include "key_proc.h"
#include "led.h"
#include "display.h"
#include "bsp_rtc.h"
#include "main.h"

static const Key_Config_t s_key_configs[6] = {
    { K1_GPIO_Port, K1_Pin, KEY_ACTIVE_LOW  },
    { K2_GPIO_Port, K2_Pin, KEY_ACTIVE_LOW  },
    { K3_GPIO_Port, K3_Pin, KEY_ACTIVE_LOW  },
    { K4_GPIO_Port, K4_Pin, KEY_ACTIVE_LOW  },
    { K5_GPIO_Port, K5_Pin, KEY_ACTIVE_HIGH },
    { K6_GPIO_Port, K6_Pin, KEY_ACTIVE_HIGH },
};

/* 时间设置字段 */
uint16_t s_set_year;
uint8_t  s_set_month, s_set_day, s_set_hour, s_set_minute, s_set_second;
static uint8_t s_cursor = 0;  /* 0=年 1=月 2=日 3=时 4=分 5=秒 */

static void enter_time_setting(void)
{
    BSP_RTC_DateTime_t dt;
    BSP_RTC_GetDateTime(&dt);
    s_set_year   = dt.year;
    s_set_month  = dt.month;
    s_set_day    = dt.day;
    s_set_hour   = dt.hour;
    s_set_minute = dt.minute;
    s_set_second = dt.second;
    s_cursor = 0;
    DISP_SetState(DISP_TIME_SETTING);
}

static void save_and_exit_time_setting(void)
{
    BSP_RTC_SetDate(s_set_year, s_set_month, s_set_day);
    BSP_RTC_SetTime(s_set_hour, s_set_minute, s_set_second);
    DISP_SetState(DISP_CLOCK);
}

static void cursor_inc(void)
{
    int *p = NULL;
    switch (s_cursor) {
    case 0: s_set_year++;   break;
    case 1: s_set_month = (s_set_month >= 12) ? 1  : s_set_month + 1;  break;
    case 2: s_set_day   = (s_set_day   >= 31) ? 1  : s_set_day   + 1;  break;
    case 3: s_set_hour  = (s_set_hour  >= 23) ? 0  : s_set_hour  + 1;  break;
    case 4: s_set_minute= (s_set_minute>= 59) ? 0  : s_set_minute+ 1;  break;
    case 5: s_set_second= (s_set_second>= 59) ? 0  : s_set_second+ 1;  break;
    }
    (void)p;
}

static void cursor_dec(void)
{
    switch (s_cursor) {
    case 0: s_set_year--;                        break;
    case 1: s_set_month = (s_set_month <= 1)  ? 12 : s_set_month - 1;  break;
    case 2: s_set_day   = (s_set_day   <= 1)  ? 31 : s_set_day   - 1;  break;
    case 3: s_set_hour  = (s_set_hour  == 0)  ? 23 : s_set_hour  - 1;  break;
    case 4: s_set_minute= (s_set_minute== 0)  ? 59 : s_set_minute- 1;  break;
    case 5: s_set_second= (s_set_second== 0)  ? 59 : s_set_second- 1;  break;
    }
}

void KeyProc_Init(void)
{
    Key_Init(s_key_configs, 6);
}

void KeyProc_Scan10ms(void)
{
    Key_Scan();
    DisplayState_t st = DISP_GetState();

    if (st == DISP_TIME_SETTING) {
        if (Key_IsShortPressed(KEY_IDX_K6)) {
            save_and_exit_time_setting();
            return;
        }
        if (Key_IsShortPressed(KEY_IDX_K2) || Key_IsRepeat(KEY_IDX_K2)) cursor_inc();
        if (Key_IsShortPressed(KEY_IDX_K3) || Key_IsRepeat(KEY_IDX_K3)) cursor_dec();
        if (Key_IsShortPressed(KEY_IDX_K4)) {
            s_cursor = (s_cursor + 1) % 6;
        }
    } else {
        /* 时钟状态 */
        if (Key_IsLongPressed(KEY_IDX_K6)) {
            enter_time_setting();
        } else if (Key_IsShortPressed(KEY_IDX_K6)) {
            DISP_SetState(DISP_CLOCK);
        }
    }
}