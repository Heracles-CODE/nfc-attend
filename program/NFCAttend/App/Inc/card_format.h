/**
 * @file    card_format.h
 * @brief   Mifare S50 扇区 0 块 1 数据格式定义
 *
 *  Block 1 (16 字节):
 *      [0..3]   CID (Card UID 4B, Big-endian,与 RC522 读出的 UID 等价)
 *      [4..7]   SID (Staff ID, Big-endian,十进制 5 位)
 *      [8..11]  PTS (Points / 剩余考勤点数, Big-endian)
 *      [12]     CTYPE (Card Type: 0=未发卡 1=员工 2=管理员)
 *      [13..15] 校验和(13=XOR(CID..CTYPE), 14/15=0xA5 5A 固定尾)
 */
#ifndef __CARD_FORMAT_H
#define __CARD_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

#define CARD_TYPE_NONE    0
#define CARD_TYPE_STAFF   1
#define CARD_TYPE_ADMIN   2

typedef struct {
    uint32_t cid;      /* 卡号 */
    uint32_t sid;      /* 工号 */
    uint16_t pts;      /* 点数 */
    uint8_t  type;     /* 卡类型 */
} CardInfo_t;

/** 从 16 字节 Block1 解码,成功返回 true */
bool CardFormat_DecodeBlock1(const uint8_t *block, CardInfo_t *out);

/** 把 CardInfo 编码为 16 字节 Block1 */
void CardFormat_EncodeBlock1(const CardInfo_t *info, uint8_t *block);

/** 默认出厂 block(全 0xFF) */
void CardFormat_BlankBlock1(uint8_t *block);

#endif /* __CARD_FORMAT_H */