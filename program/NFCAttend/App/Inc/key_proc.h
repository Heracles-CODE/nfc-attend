/**
 * @file    key_proc.h
 * @brief   按键事件分发器
 */
#ifndef __KEY_PROC_H
#define __KEY_PROC_H

#include <stdint.h>
#include "key.h"

/* 按键索引(与 main.h 引脚顺序一致) */
#define KEY_IDX_K1  0
#define KEY_IDX_K2  1
#define KEY_IDX_K3  2
#define KEY_IDX_K4  3
#define KEY_IDX_K5  4
#define KEY_IDX_K6  5

void KeyProc_Init(void);
void KeyProc_Scan10ms(void);   /* 由 keyTask 每 10ms 调用一次 */

#endif /* __KEY_PROC_H */