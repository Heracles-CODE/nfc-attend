/**
 * @file    nfc_app.h
 * @brief   RC522 应用层(寻卡 + 读 Block1)
 */
#ifndef __NFC_APP_H
#define __NFC_APP_H

#include <stdint.h>
#include <stdbool.h>
#include "card_format.h"

typedef enum {
    NFC_EVT_NONE = 0,
    NFC_EVT_CARD_VALID,      /* 读到合法卡片(校验通过) */
    NFC_EVT_CARD_INVALID,    /* 读到卡但校验失败/空卡 */
    NFC_EVT_CARD_LEAVE,      /* 卡离开 */
} NFC_Event_t;

typedef void (*NFC_Callback_t)(NFC_Event_t evt, const uint8_t uid[4], const CardInfo_t *info);

void NFC_App_Init(NFC_Callback_t cb);
void NFC_App_Update(void);    /* 由 nfcTask 每 100ms 调用 */

#endif /* __NFC_APP_H */