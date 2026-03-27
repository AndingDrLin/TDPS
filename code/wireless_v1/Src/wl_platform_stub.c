/**
 * @file    wl_platform_stub.c
 * @brief   平台抽象层桩实现（用于离线测试 / PC 端编译）。
 *
 * 当未定义 WL_USE_STM32F1_HAL_PORT 时编译此文件，
 * 提供 wl_platform.h 中所有接口的空实现或简单模拟。
 */

#ifndef WL_USE_STM32F1_HAL_PORT

#include "wl_config.h"
#include "wl_platform.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  模拟毫秒计数器                                                     */
/* ------------------------------------------------------------------ */

static uint32_t stub_millis = 0;

/* ------------------------------------------------------------------ */
/*  模拟 UART 接收缓冲区                                               */
/* ------------------------------------------------------------------ */

static uint8_t stub_rx_buf[WL_UART_RX_BUF_SIZE];
static uint16_t stub_rx_len = 0;
static uint16_t stub_rx_pos = 0;

/* ------------------------------------------------------------------ */
/*  最近一次 UART 发送缓存（供自动化测试断言）                          */
/* ------------------------------------------------------------------ */

static uint8_t stub_last_tx_buf[WL_UART_TX_BUF_SIZE];
static uint16_t stub_last_tx_len = 0;

/* ------------------------------------------------------------------ */
/*  PAL 接口桩实现                                                     */
/* ------------------------------------------------------------------ */

void WL_Platform_Init(void)
{
    stub_millis = 0;
    stub_rx_len = 0;
    stub_rx_pos = 0;
    stub_last_tx_len = 0;
    printf("[Stub] WL_Platform_Init done\n");
}

uint32_t WL_Platform_GetMillis(void)
{
    return stub_millis;
}

void WL_Platform_DelayMs(uint32_t ms)
{
    stub_millis += ms;
}

void WL_Platform_UART_Send(const uint8_t *data, uint16_t len)
{
    stub_last_tx_len = len;
    if (stub_last_tx_len > WL_UART_TX_BUF_SIZE) {
        stub_last_tx_len = WL_UART_TX_BUF_SIZE;
    }
    if (stub_last_tx_len > 0U) {
        memcpy(stub_last_tx_buf, data, stub_last_tx_len);
    }

    printf("[Stub] UART TX %u bytes: ", len);
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] >= 0x20 && data[i] < 0x7F) {
            putchar(data[i]);
        } else {
            printf("\\x%02X", data[i]);
        }
    }
    printf("\n");

    /* 模拟 AT 指令响应：如果发送的是 "AT" 开头，自动填充 "AT_OK\r\n" */
    if (len >= 2U && data[0] == 'A' && data[1] == 'T') {
        const char *resp = "AT_OK\r\n";
        uint16_t rlen = (uint16_t)strlen(resp);
        if (rlen <= WL_UART_RX_BUF_SIZE) {
            memcpy(stub_rx_buf, resp, rlen);
            stub_rx_len = rlen;
            stub_rx_pos = 0;
        }
    }
}

void WL_Platform_UART_SendString(const char *str)
{
    WL_Platform_UART_Send((const uint8_t *)str, (uint16_t)strlen(str));
}

uint16_t WL_Platform_UART_Receive(uint8_t *buf, uint16_t max_len)
{
    uint16_t count = 0;
    while (count < max_len && stub_rx_pos < stub_rx_len) {
        buf[count++] = stub_rx_buf[stub_rx_pos++];
    }
    return count;
}

void WL_Platform_UART_FlushRx(void)
{
    stub_rx_len = 0;
    stub_rx_pos = 0;
}

bool WL_Platform_ReadAUX(void)
{
    /* 桩实现始终返回高电平（模块就绪） */
    return true;
}

void WL_Platform_DebugPrint(const char *msg)
{
    printf("[Debug] %s", msg);
}

/* ------------------------------------------------------------------ */
/*  桩专用辅助函数（供测试代码调用）                                     */
/* ------------------------------------------------------------------ */

void WL_Stub_AdvanceMillis(uint32_t ms)
{
    stub_millis += ms;
}

void WL_Stub_InjectRxData(const uint8_t *data, uint16_t len)
{
    if (len > WL_UART_RX_BUF_SIZE) {
        len = WL_UART_RX_BUF_SIZE;
    }

    memcpy(stub_rx_buf, data, len);
    stub_rx_len = len;
    stub_rx_pos = 0;
}

void WL_Stub_ClearLastTx(void)
{
    stub_last_tx_len = 0;
}

uint16_t WL_Stub_GetLastTx(uint8_t *buf, uint16_t max_len)
{
    uint16_t copy_len = stub_last_tx_len;
    if (copy_len > max_len) {
        copy_len = max_len;
    }

    if (copy_len > 0U && buf != NULL) {
        memcpy(buf, stub_last_tx_buf, copy_len);
    }

    return copy_len;
}

uint16_t WL_Stub_GetLastTxLen(void)
{
    return stub_last_tx_len;
}

#endif /* !WL_USE_STM32F1_HAL_PORT */
