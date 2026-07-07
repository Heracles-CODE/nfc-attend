/**
 * @file    display.c
 * @brief   OLED 显示场景状态机实现
 *
 *  guiTask 每 50ms 调用一次 DISP_Update()。
 *  为防止 OLED 重复刷写导致的闪烁,只有状态真正改变时
 *  才执行 GUI_Clear(),内容一致时只更新数据不重复清屏。
 *
 *  状态转换:
 *    STARTUP ──40次渲染计数──▶ CLOCK
 *    CLOCK ──K6长按──▶ TIME_SETTING ──K6短按──▶ CLOCK
 *    CLOCK ──刷卡VALID──▶ CARD_INFO ──3秒无卡/LEAVE──▶ CLOCK
 *    CLOCK ──刷卡INVALID──▶ CARD_INFO(ERR) ──3秒无卡/LEAVE──▶ CLOCK
 */
#include "display.h"
#include "bsp_rtc.h"
#include "gui.h"
#include "ssd1306.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

static DisplayState_t  s_state = DISP_STARTUP;
static uint32_t       s_startup_calls = 0;  /* STARTUP 渲染计数 */

/* 上一次刷卡结果缓存 */
static uint8_t   s_last_uid[4];
static CardInfo_t s_last_info;
static uint8_t   s_last_valid;
static uint8_t   s_last_shown = 0;  /* 当前卡结果已显示过(防重复刷新) */

/* 手动刷新标志(由 DISP_ShowCardResult 置 1,DISP_ClearCardResult 清 0) */
static uint8_t   s_force_redraw = 0;

/* 时间设置临时变量 */
extern uint16_t s_set_year;
extern uint8_t  s_set_month, s_set_day, s_set_hour, s_set_minute, s_set_second;

/* STARTUP 渲染 40 次 × 50ms ≈ 2 秒 */
#define STARTUP_RENDER_COUNT  40
/* CARD_INFO 3 秒自动回 CLOCK */
#define CARD_INFO_TIMEOUT_TICKS  (3000 / 50)

void DISP_Init(void)
{
    GUI_Init();
    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_Clear();
    GUI_DispStringHCenterAt("NFC Attend", 64, 16);
    GUI_DispStringHCenterAt("v1.0 phase1", 64, 32);
    GUI_DispStringAt("Starting...", 0, 48);
    GUI_Update();
    s_state = DISP_STARTUP;
    s_startup_calls = 0;
    s_last_shown = 0;
    s_force_redraw = 0;
}

void DISP_SetState(DisplayState_t s)
{
    if (s == s_state) return;
    s_state = s;
    s_startup_calls = 0;
    s_last_shown = 0;       /* 新状态清零显示标记 */
    s_force_redraw = 1;     /* 强制刷新 */
    GUI_Clear();             /* 切状态时清屏 */
}

/* 强制切换状态(不看当前状态,直接切) */
static void force_set_state(DisplayState_t s)
{
    if (s == s_state) return;
    s_state = s;
    s_startup_calls = 0;
    s_last_shown = 0;
    s_force_redraw = 1;
    GUI_Clear();
}

DisplayState_t DISP_GetState(void) { return s_state; }

void DISP_ShowCardResult(uint8_t uid[4], const CardInfo_t *info, uint8_t isValid)
{
    memcpy(s_last_uid, uid, 4);
    if (info) s_last_info = *info;
    s_last_valid = isValid;
    s_last_shown = 0;     /* 重新标记需要显示 */
    s_force_redraw = 1;
    force_set_state(DISP_CARD_INFO);
}

void DISP_ClearCardResult(void)
{
    s_last_valid = 0;
    s_last_shown = 0;
    s_force_redraw = 1;
    force_set_state(DISP_CLOCK);
}

/* ──── 渲染函数 ──── */

static void render_startup(void)
{
    if (s_startup_calls < STARTUP_RENDER_COUNT) {
        s_startup_calls++;
        GUI_Clear();
        GUI_DispStringHCenterAt("NFC Attend", 64, 16);
        GUI_DispStringHCenterAt("v1.0 phase1", 64, 32);
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "init %lu/%lu",
                 (unsigned long)s_startup_calls,
                 (unsigned long)STARTUP_RENDER_COUNT);
        GUI_DispStringAt(tmp, 0, 48);
        s_force_redraw = 0;
    } else {
        force_set_state(DISP_CLOCK);
    }
}

static void render_clock(void)
{
    BSP_RTC_DateTime_t dt;
    BSP_RTC_GetDateTime(&dt);

    char buf[24];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u", dt.year, dt.month, dt.day);
    GUI_DispStringAt(buf, 0, 0);
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
    GUI_DispStringAt(buf, 0, 16);
    snprintf(buf, sizeof(buf), "DEV:1  MODE:%u",
             (unsigned)s_last_valid);
    GUI_DispStringAt(buf, 0, 40);
}

static void render_card_info(void)
{
    /* 3 秒后自动回 CLOCK */
    if (s_startup_calls >= CARD_INFO_TIMEOUT_TICKS) {
        force_set_state(DISP_CLOCK);
        return;
    }
    s_startup_calls++;

    /* 内容没变且已显示过 → 不重复刷屏 */
    if (!s_force_redraw && s_last_shown) {
        s_force_redraw = 0;
        return;
    }
    s_force_redraw = 0;
    s_last_shown = 1;

    GUI_Clear();
    GUI_DispStringAt(s_last_valid ? "CARD OK" : "CARD ERR", 0, 0);

    char buf[32];
    snprintf(buf, sizeof(buf), "UID:%02X%02X%02X%02X",
             s_last_uid[0], s_last_uid[1], s_last_uid[2], s_last_uid[3]);
    GUI_DispStringAt(buf, 0, 16);

    if (s_last_valid) {
        snprintf(buf, sizeof(buf), "SID:%05u", (unsigned)s_last_info.sid);
        GUI_DispStringAt(buf, 0, 32);
        snprintf(buf, sizeof(buf), "TYPE:%u  PTS:%u",
                 (unsigned)s_last_info.type, (unsigned)s_last_info.pts);
        GUI_DispStringAt(buf, 0, 48);
    } else {
        /* 无效卡时显示空,不重复刷新 */
        GUI_DispStringAt(" Pls use staff card", 0, 32);
    }
}

static void render_time_setting(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u",
             (unsigned)s_set_year, (unsigned)s_set_month, (unsigned)s_set_day);
    GUI_DispStringAt(buf, 0, 16);
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             (unsigned)s_set_hour, (unsigned)s_set_minute, (unsigned)s_set_second);
    GUI_DispStringAt(buf, 0, 32);
    GUI_DispStringAt("K2:+ K3:- K6:save", 0, 48);
}

/* ──── 主更新 ──── */

void DISP_Update(void)
{
    switch (s_state) {
    case DISP_STARTUP:      render_startup();      break;
    case DISP_CLOCK:        render_clock();        break;
    case DISP_CARD_INFO:    render_card_info();    break;
    case DISP_TIME_SETTING: render_time_setting(); break;
    default: break;
    }
    SSD1306_UpdateScreen();
}