/**
 * @file    wl_protocol.c
 * @brief   无线通信数据协议层实现 — 拱门报文打包。
 */

#include "wl_protocol.h"
#include "wl_config.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  报文构造                                                           */
/* ------------------------------------------------------------------ */

uint16_t WL_Protocol_BuildCheckpointMsg(char *buf,
                                         uint16_t buf_len,
                                         uint32_t checkpoint_id,
                                         uint32_t elapsed_ms)
{
    if (buf == NULL || buf_len == 0) {
        return 0;
    }

    int n = snprintf(buf, buf_len,
                     "TEAM=%u,NAME=%s,CP=%u,TIME=%u\n",
                     (unsigned)g_wl_config.team_number,
                     g_wl_config.team_name,
                     (unsigned)checkpoint_id,
                     (unsigned)elapsed_ms);

    if (n < 0 || (uint16_t)n >= buf_len) {
        /* 缓冲区不足，截断处理 */
        buf[buf_len - 1] = '\0';
        return (uint16_t)(buf_len - 1);
    }

    return (uint16_t)n;
}

uint16_t WL_Protocol_BuildCustomMsg(char *buf,
                                     uint16_t buf_len,
                                     const char *text)
{
    if (buf == NULL || buf_len == 0 || text == NULL) {
        return 0;
    }

    int n = snprintf(buf, buf_len,
                     "TEAM=%u,NAME=%s,MSG=%s\n",
                     (unsigned)g_wl_config.team_number,
                     g_wl_config.team_name,
                     text);

    if (n < 0 || (uint16_t)n >= buf_len) {
        buf[buf_len - 1] = '\0';
        return (uint16_t)(buf_len - 1);
    }

    return (uint16_t)n;
}
