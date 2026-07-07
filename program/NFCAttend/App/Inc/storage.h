/**
 * @file    storage.h
 * @brief   W25Q128 配置区(设备号、考勤模式)读写
 *
 * 第一阶段只读写设备号与考勤模式;考勤记录相关留待第二阶段。
 */
#ifndef __STORAGE_H
#define __STORAGE_H

#include <stdint.h>

#define STORAGE_DEV_ID_DEFAULT  1
#define STORAGE_MODE_DEFAULT    0   /* 0=进/出 1=仅签到 2=仅签退 */

void     NFC_STORAGE_Init(void);
uint16_t NFC_STORAGE_GetDevId(void);
void     NFC_STORAGE_SetDevId(uint16_t id);
uint8_t  NFC_STORAGE_GetMode(void);
void     NFC_STORAGE_SetMode(uint8_t mode);

#endif /* __STORAGE_H */