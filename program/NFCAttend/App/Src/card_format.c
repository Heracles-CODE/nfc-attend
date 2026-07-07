/**
 * @file    card_format.c
 * @brief   Block1 编解码实现
 */
#include "card_format.h"
#include <string.h>

#define CARD_TAIL_LO   0xA5
#define CARD_TAIL_HI   0x5A

void CardFormat_BlankBlock1(uint8_t *block)
{
    memset(block, 0xFF, 16);
}

bool CardFormat_DecodeBlock1(const uint8_t *block, CardInfo_t *out)
{
    if (!block || !out) return false;

    /* 校验尾 */
    if (block[14] != CARD_TAIL_LO || block[15] != CARD_TAIL_HI) {
        return false;
    }

    /* 校验和:第 13 字节 = [0..12] 异或 */
    uint8_t cs = 0;
    for (uint8_t i = 0; i < 13; i++) cs ^= block[i];
    if (cs != block[13]) return false;

    out->cid  = ((uint32_t)block[0] << 24) | ((uint32_t)block[1] << 16)
              | ((uint32_t)block[2] << 8)  |  (uint32_t)block[3];
    out->sid  = ((uint32_t)block[4] << 24) | ((uint32_t)block[5] << 16)
              | ((uint32_t)block[6] << 8)  |  (uint32_t)block[7];
    out->pts  = ((uint32_t)block[8] << 8)  |  (uint32_t)block[9];
    /* [10..11] 保留 */
    out->type = block[12];
    return true;
}

void CardFormat_EncodeBlock1(const CardInfo_t *info, uint8_t *block)
{
    if (!info || !block) return;
    memset(block, 0, 16);
    block[0]  = (info->cid >> 24) & 0xFF;
    block[1]  = (info->cid >> 16) & 0xFF;
    block[2]  = (info->cid >> 8)  & 0xFF;
    block[3]  =  info->cid        & 0xFF;
    block[4]  = (info->sid >> 24) & 0xFF;
    block[5]  = (info->sid >> 16) & 0xFF;
    block[6]  = (info->sid >> 8)  & 0xFF;
    block[7]  =  info->sid        & 0xFF;
    block[8]  = (info->pts >> 8)  & 0xFF;
    block[9]  =  info->pts        & 0xFF;
    /* [10..11] 保留为 0 */
    block[12] = info->type;
    /* 计算校验 */
    uint8_t cs = 0;
    for (uint8_t i = 0; i < 13; i++) cs ^= block[i];
    block[13] = cs;
    block[14] = CARD_TAIL_LO;
    block[15] = CARD_TAIL_HI;
}