/**
 * @file    wl_lora.c
 * @brief   EWM22A-900BWL22S LoRa 驱动层实现。
 *
 * 通过 AT 指令完成模块配置，通过 UART 透传完成数据发射。
 */

#include "wl_lora.h"
#include "wl_platform.h"
#include "wl_config.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  内部常量                                                           */
/* ------------------------------------------------------------------ */

/** AT 指令响应中表示成功的关键字。 */
#define AT_OK_TOKEN     "AT_OK"

/** 临时缓冲区大小（用于构造 AT 指令和读取响应）。 */
#define CMD_BUF_SIZE    64
#define RESP_BUF_SIZE   128

/* ------------------------------------------------------------------ */
/*  等待 AUX 就绪                                                      */
/* ------------------------------------------------------------------ */

bool WL_LoRa_WaitAUX(uint32_t timeout_ms)
{
    uint32_t start = WL_Platform_GetMillis();
    while (!WL_Platform_ReadAUX()) {
        if ((WL_Platform_GetMillis() - start) >= timeout_ms) {
            WL_Platform_DebugPrint("[LoRa] 等待 AUX 超时\r\n");
            return false;
        }
        WL_Platform_DelayMs(1);
    }
    return true;
}

bool WL_LoRa_IsReady(void)
{
    return WL_Platform_ReadAUX();
}

/* ------------------------------------------------------------------ */
/*  AT 指令收发                                                        */
/* ------------------------------------------------------------------ */

WL_LoRa_Status WL_LoRa_SendAT(const char *cmd,
                                char *resp_buf,
                                uint16_t resp_buf_len,
                                uint32_t timeout_ms)
{
    /* 清空接收缓冲区，避免残留数据干扰 */
    WL_Platform_UART_FlushRx();

    /* 发送 AT 指令（EWM22A 的 AT 指令不带回车换行） */
    WL_Platform_UART_SendString(cmd);

    /* 等待并收集响应 */
    char local_buf[RESP_BUF_SIZE];
    uint16_t pos = 0;
    uint32_t start = WL_Platform_GetMillis();

    memset(local_buf, 0, sizeof(local_buf));

    while ((WL_Platform_GetMillis() - start) < timeout_ms) {
        uint8_t byte;
        uint16_t n = WL_Platform_UART_Receive(&byte, 1);
        if (n > 0) {
            if (pos < RESP_BUF_SIZE - 1) {
                local_buf[pos++] = (char)byte;
                local_buf[pos] = '\0';
            }
            /* 检查是否已收到 AT_OK */
            if (strstr(local_buf, AT_OK_TOKEN) != NULL) {
                if (resp_buf != NULL && resp_buf_len > 0) {
                    uint16_t copy_len = pos < resp_buf_len ? pos : (resp_buf_len - 1);
                    memcpy(resp_buf, local_buf, copy_len);
                    resp_buf[copy_len] = '\0';
                }

                /* 调试输出 */
                char dbg[CMD_BUF_SIZE + 32];
                snprintf(dbg, sizeof(dbg), "[LoRa] AT 成功: %s\r\n", cmd);
                WL_Platform_DebugPrint(dbg);
                return WL_LORA_OK;
            }
            /* 检查是否有错误响应 */
            if (strstr(local_buf, "AT_PARAM_ERROR") != NULL ||
                strstr(local_buf, "AT_ERROR") != NULL) {
                char dbg[CMD_BUF_SIZE + 32];
                snprintf(dbg, sizeof(dbg), "[LoRa] AT 失败: %s -> %s\r\n",
                         cmd, local_buf);
                WL_Platform_DebugPrint(dbg);
                return WL_LORA_ERR_RESPONSE;
            }
        } else {
            WL_Platform_DelayMs(1);
        }
    }

    /* 超时未收到有效响应 */
    char dbg[CMD_BUF_SIZE + 32];
    snprintf(dbg, sizeof(dbg), "[LoRa] AT 超时: %s\r\n", cmd);
    WL_Platform_DebugPrint(dbg);
    return WL_LORA_ERR_TIMEOUT;
}

/* ------------------------------------------------------------------ */
/*  内部辅助：发送一条 AT 指令并检查结果                                 */
/* ------------------------------------------------------------------ */

static WL_LoRa_Status _send_at_check(const char *cmd)
{
    return WL_LoRa_SendAT(cmd, NULL, 0, g_wl_config.at_response_timeout_ms);
}

/* ------------------------------------------------------------------ */
/*  模块初始化                                                         */
/* ------------------------------------------------------------------ */

WL_LoRa_Status WL_LoRa_Init(void)
{
    WL_LoRa_Status st;
    char cmd[CMD_BUF_SIZE];

    WL_Platform_DebugPrint("[LoRa] 开始初始化...\r\n");

    /* 1. 等待模块上电自检完成（AUX 拉高） */
    WL_Platform_DelayMs(g_wl_config.power_on_delay_ms);
    if (!WL_LoRa_WaitAUX(g_wl_config.at_response_timeout_ms)) {
        WL_Platform_DebugPrint("[LoRa] 模块未就绪，初始化失败\r\n");
        return WL_LORA_ERR_TIMEOUT;
    }

    /* 2. 进入配置模式（模式 0） */
    st = _send_at_check("AT+HMODE=0");
    if (st != WL_LORA_OK) return st;
    WL_Platform_DelayMs(g_wl_config.mode_switch_delay_ms);
    WL_LoRa_WaitAUX(g_wl_config.at_response_timeout_ms);

    /* 3. 验证 AT 链路 */
    st = _send_at_check("AT");
    if (st != WL_LORA_OK) return st;

    /* 4. 设置模块地址 */
    snprintf(cmd, sizeof(cmd), "AT+ADDR=%u",
             (unsigned)g_wl_config.lora_addr);
    st = _send_at_check(cmd);
    if (st != WL_LORA_OK) return st;

    /* 5. 设置信道 */
    snprintf(cmd, sizeof(cmd), "AT+CHANNEL=%u",
             (unsigned)g_wl_config.lora_channel);
    st = _send_at_check(cmd);
    if (st != WL_LORA_OK) return st;

    /* 6. 设置网络 ID */
    snprintf(cmd, sizeof(cmd), "AT+NETID=%u",
             (unsigned)g_wl_config.lora_net_id);
    st = _send_at_check(cmd);
    if (st != WL_LORA_OK) return st;

    /* 7. 设置空中速率 */
    snprintf(cmd, sizeof(cmd), "AT+RATE=%u",
             (unsigned)g_wl_config.lora_air_rate);
    st = _send_at_check(cmd);
    if (st != WL_LORA_OK) return st;

    /* 8. 设置发射功率 */
    snprintf(cmd, sizeof(cmd), "AT+POWER=%u",
             (unsigned)g_wl_config.lora_tx_power);
    st = _send_at_check(cmd);
    if (st != WL_LORA_OK) return st;

    /* 9. 设置传输模式（透明 / 定点） */
    snprintf(cmd, sizeof(cmd), "AT+TRANS=%u",
             (unsigned)g_wl_config.lora_trans_mode);
    st = _send_at_check(cmd);
    if (st != WL_LORA_OK) return st;

    /* 10. 设置分包长度 */
    snprintf(cmd, sizeof(cmd), "AT+PACKET=%u",
             (unsigned)g_wl_config.lora_packet_size);
    st = _send_at_check(cmd);
    if (st != WL_LORA_OK) return st;

    /* 11. 切换到 UART/LoRa 透传模式（模式 1），BLE 监听 */
    st = _send_at_check("AT+HMODE=1");
    if (st != WL_LORA_OK) return st;
    WL_Platform_DelayMs(g_wl_config.mode_switch_delay_ms);
    WL_LoRa_WaitAUX(g_wl_config.at_response_timeout_ms);

    WL_Platform_DebugPrint("[LoRa] 初始化完成，已进入透传模式\r\n");
    return WL_LORA_OK;
}

/* ------------------------------------------------------------------ */
/*  数据发送                                                           */
/* ------------------------------------------------------------------ */

WL_LoRa_Status WL_LoRa_Send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return WL_LORA_ERR_PARAM;
    }
    if (len > WL_TX_PAYLOAD_MAX) {
        WL_Platform_DebugPrint("[LoRa] 发送数据超过最大载荷长度\r\n");
        return WL_LORA_ERR_PARAM;
    }

    /* 等待模块就绪 */
    if (!WL_LoRa_WaitAUX(g_wl_config.at_response_timeout_ms)) {
        return WL_LORA_ERR_BUSY;
    }

    /* 在透传模式下，直接通过 UART 发送数据即可由模块进行 LoRa 发射 */
    WL_Platform_UART_Send(data, len);

    char dbg[48];
    snprintf(dbg, sizeof(dbg), "[LoRa] 已发送 %u 字节\r\n", (unsigned)len);
    WL_Platform_DebugPrint(dbg);

    return WL_LORA_OK;
}

WL_LoRa_Status WL_LoRa_SendString(const char *str)
{
    if (str == NULL) {
        return WL_LORA_ERR_PARAM;
    }
    return WL_LoRa_Send((const uint8_t *)str, (uint16_t)strlen(str));
}
