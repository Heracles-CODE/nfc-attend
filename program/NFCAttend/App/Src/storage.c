/**
 * @file    storage.c
 * @brief   W25Q128 设备号/考勤模式持久化
 *
 * 第一阶段:
 *   - 0x0000: 设备号 (2B) + 魔数 (2B)
 *   - 0x0010: 考勤模式 (1B) + 魔数 (1B)
 *   - 其他扇区保留给第二阶段的考勤记录
 */
#include "storage.h"
#include "w25qxx.h"
#include <string.h>

#define ADDR_DEV_ID   0x0000
#define ADDR_MODE     0x0010
#define MAGIC         0x5050u

static uint16_t s_dev_id = STORAGE_DEV_ID_DEFAULT;
static uint8_t  s_mode   = STORAGE_MODE_DEFAULT;

void NFC_STORAGE_Init(void)
{
    /* 读设备号(纯读,无写操作,可在任务中安全调用) */
    uint16_t buf[2];
    W25QXX_Read((uint8_t *)buf, ADDR_DEV_ID, sizeof(buf));
    if (buf[1] == MAGIC) {
        s_dev_id = buf[0];
    } else {
        s_dev_id = STORAGE_DEV_ID_DEFAULT;  /* 首次用默认值,不写入 */
    }

    /* 读模式 */
    uint8_t modeBuf[2];
    W25QXX_Read(modeBuf, ADDR_MODE, sizeof(modeBuf));
    if (modeBuf[1] == (MAGIC & 0xFF)) {
        s_mode = modeBuf[0];
    } else {
        s_mode = STORAGE_MODE_DEFAULT;
    }
}

uint16_t NFC_STORAGE_GetDevId(void) { return s_dev_id; }

void NFC_STORAGE_SetDevId(uint16_t id)
{
    s_dev_id = id;
    uint16_t buf[2] = { id, MAGIC };
    W25QXX_Erase_Sector(0);
    W25QXX_Wait_Busy();
    W25QXX_Write_NoCheck((uint8_t *)buf, ADDR_DEV_ID, sizeof(buf));
    W25QXX_Wait_Busy();
}

uint8_t NFC_STORAGE_GetMode(void) { return s_mode; }

void NFC_STORAGE_SetMode(uint8_t mode)
{
    s_mode = mode;
    uint8_t buf[2] = { mode, (uint8_t)(MAGIC & 0xFF) };
    W25QXX_Erase_Sector(0);
    W25QXX_Wait_Busy();
    W25QXX_Write_NoCheck(buf, ADDR_MODE, sizeof(buf));
    W25QXX_Wait_Busy();
}