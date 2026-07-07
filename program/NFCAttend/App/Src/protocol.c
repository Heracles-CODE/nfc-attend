/**
 * @file    protocol.c
 * @brief   PC↔STM32 命令协议解析层实现
 *
 *  命令以\n结尾,状态机: 收字符→\n触发解析→执行→清缓冲
 */
#include "protocol.h"
#include "rc522.h"
#include "card_format.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char   s_buf[PROTO_BUF_SIZE];
static uint16_t s_len = 0;

/* 默认密码 */
static const uint8_t DEF_KEY[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* 当前操作卡的 UID(4B) */
static uint8_t s_uid[4];

/* 图像临时缓冲: 最多 24 块 × 16 字节 = 384 B */
static uint8_t s_img_buf[24 * 16];
static uint8_t s_img_sector;  /* 当前写入的起始扇区号 */

void PROTOCOL_Init(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    s_len = 0;
    memset(s_uid, 0, sizeof(s_uid));
}

/* ──── 内部辅助 ──── */
static void proto_send(const char *str)
{
    /* 通过 USART1 发送(printf 已重定向) */
    printf("%s", str);
}

/* HEX 32字符 → 16字节,返回转换成功字节数 */
static int hex32_to_bin(const char *hex, uint8_t *out)
{
    for (int i = 0; i < 16; i++) {
        char h = hex[i * 2], l = hex[i * 2 + 1];
        uint8_t val = 0;
        if (h >= '0' && h <= '9') val = (h - '0') << 4;
        else if (h >= 'A' && h <= 'F') val = (h - 'A' + 10) << 4;
        else if (h >= 'a' && h <= 'f') val = (h - 'a' + 10) << 4;
        else return -1;
        if (l >= '0' && l <= '9') val |= (l - '0');
        else if (l >= 'A' && l <= 'F') val |= (l - 'A' + 10);
        else if (l >= 'a' && l <= 'f') val |= (l - 'a' + 10);
        else return -1;
        out[i] = val;
    }
    return 16;
}

/* 寻卡 + 认证, 返回 0=成功 */
static int proto_scan_and_auth(void)
{
    char st;
    st = RC522_ScanCard(s_uid);
    if (st != RC522_OK) { proto_send("ERR:NOCARD\r\n"); return -1; }

    st = RC522_AuthState(RC522_PICC_AUTHENT1A, 3, (uint8_t *)DEF_KEY, s_uid);
    if (st != RC522_OK) { proto_send("ERR:AUTH\r\n"); return -1; }
    return 0;
}

/* ──── 命令分发 ──── */
static void cmd_issue(const char *args)
{
    /* 格式: CID_HEX,SID_DEC,PTS,CTYPE */
    uint32_t cid = 0, sid = 0, pts = 0, ctype = 0;
    if (sscanf(args, "%lx,%lu,%lu,%lu", &cid, &sid, &pts, &ctype) < 4) {
        proto_send("ERR:PARSE\r\n"); return;
    }
    if (proto_scan_and_auth() != 0) return;

    CardInfo_t info;
    info.cid  = cid;
    info.sid  = sid;
    info.pts  = (uint16_t)pts;
    info.type = (uint8_t)ctype;

    uint8_t block[16];
    CardFormat_EncodeBlock1(&info, block);

    char st = RC522_WriteBlock(0, 1, block);
    if (st != RC522_OK) {
        proto_send("ERR:WRITE_B1\r\n"); return;
    }
    proto_send("OK\r\n");
}

static void cmd_read(void)
{
    if (proto_scan_and_auth() != 0) return;

    uint8_t block[16];
    char st = RC522_ReadBlock(0, 1, block);
    if (st != RC522_OK) {
        proto_send("ERR:AUTH\r\n"); return;
    }

    CardInfo_t info;
    if (!CardFormat_DecodeBlock1(block, &info)) {
        /* 未发卡:返回 UID */
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "UID:%02X%02X%02X%02X\r\n",
                 s_uid[0], s_uid[1], s_uid[2], s_uid[3]);
        proto_send(buf);
        return;
    }

    char buf[128];
    snprintf(buf, sizeof(buf),
             "UID:%02X%02X%02X%02X\r\n"
             "SID:%05lu\r\n"
             "PTS:%u\r\n"
             "TYPE:%u\r\n"
             "OK\r\n",
             s_uid[0], s_uid[1], s_uid[2], s_uid[3],
             (unsigned long)info.sid,
             (unsigned)info.pts,
             (unsigned)info.type);
    proto_send(buf);
}

static void cmd_imga(const char *args)
{
    /* IMGAxx:HEX32 → 块号 xx 映射到扇区1~8 */
    int blk;
    if (sscanf(args, "%d:", &blk) < 1 || blk < 0 || blk > 23) {
        proto_send("ERR:IMG_PARSE\r\n"); return;
    }
    const char *hex = strchr(args, ':');
    if (!hex) { proto_send("ERR:IMG_PARSE\r\n"); return; }
    hex++;

    uint8_t bin[16];
    if (hex32_to_bin(hex, bin) != 16) {
        proto_send("ERR:IMG_HEX\r\n"); return;
    }

    /* 扇区1~8,每扇区3个数据块:块号0~2→扇区1,3~5→扇区2...
       总块索引 = 4(扇区1起始块) + blk + blk/3
       简化: 扇区 = 1 + blk/3, 块 = blk % 3  */
    uint8_t sec  = 1 + blk / 3;
    uint8_t blkn = blk % 3;

    if (proto_scan_and_auth() != 0) return;
    /* 重新认证目标扇区 */
    char st = RC522_AuthState(RC522_PICC_AUTHENT1A,
                               sec * 4 + 3, (uint8_t *)DEF_KEY, s_uid);
    if (st != RC522_OK) { proto_send("ERR:AUTH\r\n"); return; }

    st = RC522_WriteBlock(sec, blkn, bin);
    if (st != RC522_OK) { proto_send("ERR:IMG_BLOCK\r\n"); return; }
    proto_send("OK\r\n");
}

static void cmd_imgn(const char *args)
{
    int blk;
    if (sscanf(args, "%d:", &blk) < 1 || blk < 0 || blk > 9) {
        proto_send("ERR:IMG_PARSE\r\n"); return;
    }
    const char *hex = strchr(args, ':');
    if (!hex) { proto_send("ERR:IMG_PARSE\r\n"); return; }
    hex++;

    uint8_t bin[16];
    if (hex32_to_bin(hex, bin) != 16) {
        proto_send("ERR:IMG_HEX\r\n"); return;
    }

    /* 扇区9~11,每扇区3块+1密钥块:块0~2→扇9,3~5→扇10,6~9跨扇 */
    uint8_t sec  = 9 + blk / 3;
    uint8_t blkn = blk % 3;

    if (proto_scan_and_auth() != 0) return;
    char st = RC522_AuthState(RC522_PICC_AUTHENT1A,
                               sec * 4 + 3, (uint8_t *)DEF_KEY, s_uid);
    if (st != RC522_OK) { proto_send("ERR:AUTH\r\n"); return; }

    st = RC522_WriteBlock(sec, blkn, bin);
    if (st != RC522_OK) { proto_send("ERR:IMG_BLOCK\r\n"); return; }
    proto_send("OK\r\n");
}

static void cmd_imgd(const char *args)
{
    int blk;
    if (sscanf(args, "%d:", &blk) < 1 || blk < 0 || blk > 9) {
        proto_send("ERR:IMG_PARSE\r\n"); return;
    }
    const char *hex = strchr(args, ':');
    if (!hex) { proto_send("ERR:IMG_PARSE\r\n"); return; }
    hex++;

    uint8_t bin[16];
    if (hex32_to_bin(hex, bin) != 16) {
        proto_send("ERR:IMG_HEX\r\n"); return;
    }

    uint8_t sec  = 12 + blk / 3;
    uint8_t blkn = blk % 3;

    if (proto_scan_and_auth() != 0) return;
    char st = RC522_AuthState(RC522_PICC_AUTHENT1A,
                               sec * 4 + 3, (uint8_t *)DEF_KEY, s_uid);
    if (st != RC522_OK) { proto_send("ERR:AUTH\r\n"); return; }

    st = RC522_WriteBlock(sec, blkn, bin);
    if (st != RC522_OK) { proto_send("ERR:IMG_BLOCK\r\n"); return; }
    proto_send("OK\r\n");
}

static void cmd_updateimg(void)
{
    proto_send("OK\r\n");
}

static void cmd_clear(const char *args)
{
    /* 清除 Block1(写入全 0xFF) */
    uint8_t blank[16];
    CardFormat_BlankBlock1(blank);

    if (proto_scan_and_auth() != 0) return;

    char st = RC522_WriteBlock(0, 1, blank);
    if (st != RC522_OK) { proto_send("ERR:WRITE_B1\r\n"); return; }
    proto_send("OK\r\n");
}

static void cmd_list(const char *args)
{
    /* Phase2: 从 W25Q128 读考勤记录列表,本期返回空 */
    if (strncmp(args, "ALL", 3) == 0) {
        proto_send("LIST:END\r\n");
    } else {
        proto_send("LIST:END\r\n");
    }
}

/* ──── 主分发 ──── */
static void dispatch(const char *line)
{
    if (strncmp(line, "ISSUE:", 6) == 0) {
        cmd_issue(line + 6);
    } else if (strcmp(line, "READ") == 0) {
        cmd_read();
    } else if (strncmp(line, "IMGA", 4) == 0) {
        cmd_imga(line + 4);
    } else if (strncmp(line, "IMGN", 4) == 0) {
        cmd_imgn(line + 4);
    } else if (strncmp(line, "IMGD", 4) == 0) {
        cmd_imgd(line + 4);
    } else if (strcmp(line, "UPDATEIMG") == 0) {
        cmd_updateimg();
    } else if (strncmp(line, "CLEAR:", 6) == 0) {
        cmd_clear(line + 6);
    } else if (strncmp(line, "LIST", 4) == 0) {
        cmd_list(line + 4);
    } else {
        proto_send("ERR:UNKNOWN_CMD\r\n");
    }
}

/* ──── 公开 API ──── */
void PROTOCOL_FeedByte(uint8_t ch)
{
    if (ch == '\r') return; /* 忽略 CR */
    if (ch == '\n') {
        if (s_len > 0) {
            s_buf[s_len] = '\0';
            dispatch(s_buf);
            s_len = 0;
        }
        return;
    }
    if (s_len < PROTO_BUF_SIZE - 1) {
        s_buf[s_len++] = (char)ch;
    }
}

void PROTOCOL_Process(void)
{
    /* Phase2: 可加超时保护,本期空 */
}