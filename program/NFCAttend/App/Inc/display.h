/**
 * @file    display.h
 * @brief   OLED 显示场景状态机
 */
#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <stdint.h>
#include "card_format.h"

typedef enum {
    DISP_STARTUP = 0,
    DISP_CLOCK,
    DISP_CARD_INFO,
    DISP_TIME_SETTING,
    DISP_ADMIN_SETTING,
} DisplayState_t;

void     DISP_Init(void);
void     DISP_SetState(DisplayState_t s);
DisplayState_t DISP_GetState(void);
void     DISP_Update(void);   /* 由 guiTask 周期调用 */

/* 事件注入:由 nfcTask 调用,把刷卡结果丢给 GUI */
void     DISP_ShowCardResult(uint8_t uid[4], const CardInfo_t *info, uint8_t isValid);
void     DISP_ClearCardResult(void);

#endif /* __DISPLAY_H */