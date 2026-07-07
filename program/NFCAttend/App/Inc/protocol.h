/**
 * @file    protocol.h
 * @brief   PC↔STM32 命令协议解析层
 *
 *  USART1 接收 ASCII 文本行(\n结尾),解析并执行发卡/读卡/清卡命令。
 *
 *  命令格式:
 *    ISSUE:CID_HEX,SID,PTS,CTYPE\n   发卡头
 *    IMGAxx:HEX32\n                   头像块xx(00~23)
 *    IMGNxx:HEX32\n                   姓名块xx(00~09)
 *    IMGDxx:HEX32\n                   部门块xx(00~09)
 *    UPDATEIMG\n                      完成图像写入
 *    READ\n                           读卡
 *    LIST:ALL\n 或 LIST:N\n           列出记录
 *    CLEAR:UID_HEX\n                  清卡
 */
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

#define PROTO_BUF_SIZE  256    /* 命令缓冲区最大字节数 */

/* 初始化协议层 */
void PROTOCOL_Init(void);

/* 喂入一个字节,由 uartTask 在串口接收回调中调用 */
void PROTOCOL_FeedByte(uint8_t ch);

/* 周期处理(检查超时等),由 uartTask 调用 */
void PROTOCOL_Process(void);

#endif /* __PROTOCOL_H */