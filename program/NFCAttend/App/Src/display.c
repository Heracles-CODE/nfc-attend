/**
 * @file    display.c
 * @brief   OLED 显示场景状态机 v2.0
 *
 *  场景: STARTUP(课程/项目/学号) → CLOCK(实时时钟)
 *        → CARD_INFO(工号/姓名/部门/时间/头像预留)
 *        → TIME_SETTING(按键设时)
 *
 *  guiTask 50ms 调用一次 DISP_Update()
 */
#include "display.h"
#include "bsp_rtc.h"
#include "gui.h"
#include "ssd1306.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* ──── 启动屏可配置信息 ──── */
#define STARTUP_LINE1  "NFC考勤系统 v2.0"
#define STARTUP_LINE2  "专业实践综合设计II"
#define STARTUP_LINE3  "HDU 杭州电子科技大学"
#define STARTUP_LINE4  "下位机 Phase2"

/* ──── 状态变量 ──── */
static DisplayState_t  s_state = DISP_STARTUP;
static uint32_t       s_startup_calls = 0;
static uint32_t       s_cardinfo_ticks = 0;

/* 刷卡结果缓存 */
static uint8_t   s_last_uid[4];
static CardInfo_t s_last_info;
static uint8_t   s_last_valid;
static uint8_t   s_last_shown = 0;
static uint8_t   s_force_redraw = 0;

/* 时间设置变量(extern 自 key_proc) */
extern uint16_t s_set_year;
extern uint8_t  s_set_month, s_set_day, s_set_hour, s_set_minute, s_set_second;

#define STARTUP_RENDER_COUNT    40   /* 40×50ms=2s */
#define CARDINFO_TIMEOUT_COUNT  60   /* 60×50ms=3s */

/* ──── 工具函数 ──── */
static void force_set_state(DisplayState_t s)
{
    if (s == s_state) return;
    s_state = s;
    s_startup_calls = 0;
    s_cardinfo_ticks = 0;
    s_last_shown = 0;
    s_force_redraw = 1;
    GUI_Clear();
}

static void draw_hline(int y) {
    GUI_DrawLine(0, y, 127, y);
}

/* ──── 对外 API ──── */
void DISP_Init(void)
{
    GUI_Init();
    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_Clear();
    s_state = DISP_STARTUP;
    s_startup_calls = 0;
    s_cardinfo_ticks = 0;
    s_last_shown = 0;
    s_force_redraw = 1;
}

void DISP_SetState(DisplayState_t s) { force_set_state(s); }
DisplayState_t DISP_GetState(void) { return s_state; }

void DISP_ShowCardResult(uint8_t uid[4], const CardInfo_t *info, uint8_t isValid)
{
    memcpy(s_last_uid, uid, 4);
    if (info) s_last_info = *info;
    s_last_valid = isValid;
    s_last_shown = 0;
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

/* ──── render_startup: 课程/项目/学号/杭电LOGO ──── */
static void render_startup(void)
{
    if (s_startup_calls >= STARTUP_RENDER_COUNT) {
        force_set_state(DISP_CLOCK);
        return;
    }
    s_startup_calls++;

    GUI_Clear();
    /* 第1行: 项目名称 */
    GUI_DispStringHCenterAt(STARTUP_LINE1, 64, 0);
    /* 第2行: 课程名称 */
    GUI_DispStringHCenterAt(STARTUP_LINE2, 64, 12);
    /* 第3行: 学校 */
    GUI_DispStringHCenterAt(STARTUP_LINE3, 64, 24);
    /* 分隔线 */
    draw_hline(38);
    /* 第4行: 版本/阶段 */
    GUI_DispStringHCenterAt(STARTUP_LINE4, 64, 42);
    /* 进度条 */
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "Loading %lu/%lu",
             (unsigned long)s_startup_calls, (unsigned long)STARTUP_RENDER_COUNT);
    GUI_DispStringHCenterAt(tmp, 64, 54);
    s_force_redraw = 0;
}

/* ──── render_clock: 待机时钟界面 ──── */
static void render_clock(void)
{
    BSP_RTC_DateTime_t dt;
    BSP_RTC_GetDateTime(&dt);

    static uint8_t last_sec = 0xFF;
    if (dt.second == last_sec && !s_force_redraw) return;
    last_sec = dt.second;
    s_force_redraw = 0;

    char buf[24];
    const char *weekdays[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

    GUI_Clear();
    /* 日期行 */
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %s",
             dt.year, dt.month, dt.day, weekdays[dt.weekday % 7]);
    GUI_DispStringHCenterAt(buf, 64, 0);

    /* 大时间: y=14~36 区域 */
    GUI_SetFont(&GUI_Font8_ASCII); /* use default font */
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             dt.hour, dt.minute, dt.second);
    GUI_DispStringHCenterAt(buf, 64, 20);

    /* 底栏 */
    draw_hline(44);
    snprintf(buf, sizeof(buf), "DEV:1  NFC Attend");
    GUI_DispStringHCenterAt(buf, 64, 48);

    /* 按键提示 */
    GUI_DispStringAt("K6:Set", 0, 56);
}

/* ──── render_card_info: 刷卡结果 + 时间 ──── */
static void render_card_info(void)
{
    if (s_cardinfo_ticks >= CARDINFO_TIMEOUT_COUNT) {
        force_set_state(DISP_CLOCK);
        return;
    }
    s_cardinfo_ticks++;

    /* 非首次重绘跳过,防止 OLED 闪烁 */
    if (!s_force_redraw && s_last_shown) {
        s_force_redraw = 0;
        return;
    }
    s_force_redraw = 0;
    s_last_shown = 1;

    BSP_RTC_DateTime_t dt;
    BSP_RTC_GetDateTime(&dt);
    char buf[32];

    GUI_Clear();

    if (s_last_valid) {
        /* ──── 已发卡(合法) ──── */
        snprintf(buf, sizeof(buf), "SID:%05lu  OK",
                 (unsigned long)s_last_info.sid);
        GUI_DispStringAt(buf, 0, 0);

        draw_hline(11);
        /* 姓名占位: 实际姓名存于扇区9~11的图像块,Phase2从卡读出后显示 */
        GUI_DispStringAt("Name:-------", 0, 14);
        /* 部门占位 */
        GUI_DispStringAt("Dept:-------", 0, 26);
        draw_hline(39);

        /* 类型 */
        snprintf(buf, sizeof(buf), "T:%u PTS:%u",
                 (unsigned)s_last_info.type,
                 (unsigned)s_last_info.pts);
        GUI_DispStringAt(buf, 0, 42);

        /* 时间(右下角) */
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                 dt.hour, dt.minute, dt.second);
        GUI_DispStringAt(buf, 64, 42);

        /* 图标:右上角画头像框(48x64 占位) — Phase2 用卡内图像数据填充 */
        GUI_DrawRect(80, 0, 127, 63);

    } else {
        /* ──── 无效卡 ──── */
        GUI_DispStringAt("CARD ERROR", 0, 0);
        draw_hline(11);

        snprintf(buf, sizeof(buf), "UID:%02X%02X%02X%02X",
                 s_last_uid[0], s_last_uid[1],
                 s_last_uid[2], s_last_uid[3]);
        GUI_DispStringAt(buf, 0, 16);

        GUI_DispStringAt("Unissued card", 0, 30);
        GUI_DispStringAt("Use PC to issue", 0, 42);

        snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                 dt.hour, dt.minute, dt.second);
        GUI_DispStringAt(buf, 64, 52);
    }
}

/* ──── render_time_setting ──── */
static void render_time_setting(void)
{
    char buf[32];
    GUI_Clear();
    GUI_DispStringAt(">> Set Time <<", 0, 0);
    draw_hline(11);

    snprintf(buf, sizeof(buf), "%04u-%02u-%02u",
             (unsigned)s_set_year, (unsigned)s_set_month, (unsigned)s_set_day);
    GUI_DispStringAt(buf, 0, 16);
    snprintf(buf, sizeof(buf), "    %02u:%02u:%02u",
             (unsigned)s_set_hour, (unsigned)s_set_minute, (unsigned)s_set_second);
    GUI_DispStringAt(buf, 0, 32);

    draw_hline(44);
    GUI_DispStringAt("K2:+ K3:- K4:->", 0, 48);
    GUI_DispStringAt("K6:Save", 0, 56);
}

/* ──── 主更新入口 ──── */
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