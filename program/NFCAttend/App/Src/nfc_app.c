/**
 * @file    nfc_app.c
 * @brief   RC522 寻卡 + 读 Block1 + 校验 + 反馈
 *
 *  默认密码:FF FF FF FF FF FF
 *  寻卡→认证扇区0→读块1→CardFormat_DecodeBlock1 校验
 *
 *  防抖设计:
 *    - INVALID/VALID 事件只在卡首次出现时触发一次,
 *      卡持续在位时不重复触发(防止每100ms重触发导致的闪烁/蜂鸣器响个不停)
 *    - 连续 3 次扫描都读不到同一张卡才判 LEAVE
 */
#include "nfc_app.h"
#include "rc522.h"
#include "delay_us.h"
#include "main.h"
#include <string.h>

#define CARD_MISS_THRESHOLD   3   /* 连续 N 次检测不到同一卡号才判 LEAVE */

static NFC_Callback_t s_cb = NULL;

static uint8_t  s_uid[4];
static uint8_t  s_card_present = 0;
static uint8_t  s_notified_invalid = 0;  /* 当前卡已通知过 INVALID */
static uint8_t  s_notified_valid   = 0;  /* 当前卡已通知过 VALID   */
static uint8_t  s_miss_cnt = 0;           /* 连续失联计数 */

/* 默认密码 */
static const uint8_t DEFAULT_KEY[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void NFC_App_Init(NFC_Callback_t cb)
{
    s_cb = cb;
    s_card_present = 0;
    s_notified_invalid = 0;
    s_notified_valid = 0;
    s_miss_cnt = 0;
    memset(s_uid, 0, sizeof(s_uid));

    /* 平台初始化(DWT + GPIO + RC522 上电) */
    RC522_Platform_Init();
    RC522_ConfigISOType('A');
    RC522_AntennaOn();
}

void NFC_App_Update(void)
{
    char status;
    uint8_t cur_uid[4];

    if (s_card_present) {
        /* 已有卡:检测是否还在(防止抖动) */
        status = RC522_ScanCard(cur_uid);
        if (status == RC522_OK && memcmp(cur_uid, s_uid, 4) == 0) {
            s_miss_cnt = 0;  /* 卡还在,重置失联计数 */
            return;
        }

        /* 卡不见了,连续 CARD_MISS_THRESHOLD 次才判 LEAVE */
        s_miss_cnt++;
        if (s_miss_cnt < CARD_MISS_THRESHOLD) {
            return;  /* 偶发,再等下次确认 */
        }

        /* 确定离卡 */
        s_card_present = 0;
        s_notified_invalid = 0;
        s_notified_valid = 0;
        s_miss_cnt = 0;
        if (s_cb) s_cb(NFC_EVT_CARD_LEAVE, NULL, NULL);
        return;
    }

    /* 无卡:主动寻卡 */
    status = RC522_ScanCard(cur_uid);
    if (status != RC522_OK) return;

    /* 复制 UID,标记卡在场 */
    memcpy(s_uid, cur_uid, 4);
    s_card_present = 1;
    s_miss_cnt = 0;
    s_notified_invalid = 0;
    s_notified_valid = 0;

    /* 认证扇区 0 (块 3 = 密钥块) */
    status = RC522_AuthState(RC522_PICC_AUTHENT1A, 3,
                             (uint8_t *)DEFAULT_KEY, s_uid);
    if (status != RC522_OK) {
        /* 已通知过则跳过,防止重复触发 */
        if (!s_notified_invalid) {
            s_notified_invalid = 1;
            if (s_cb) s_cb(NFC_EVT_CARD_INVALID, s_uid, NULL);
        }
        return;
    }

    /* 读 Block 1 */
    uint8_t block[16];
    status = RC522_ReadBlock(0, 1, block);
    /* 注意:不调 RC522_Halt(),让卡保持在场,
       下一轮 scan 检测到相同 UID 会走 "卡还在" 分支 */

    if (status != RC522_OK) {
        if (!s_notified_invalid) {
            s_notified_invalid = 1;
            if (s_cb) s_cb(NFC_EVT_CARD_INVALID, s_uid, NULL);
        }
        return;
    }

    CardInfo_t info;
    if (!CardFormat_DecodeBlock1(block, &info)) {
        if (!s_notified_invalid) {
            s_notified_invalid = 1;
            if (s_cb) s_cb(NFC_EVT_CARD_INVALID, s_uid, NULL);
        }
        return;
    }

    /* 有效卡 */
    if (!s_notified_valid) {
        s_notified_valid = 1;
        if (s_cb) s_cb(NFC_EVT_CARD_VALID, s_uid, &info);
    }
}