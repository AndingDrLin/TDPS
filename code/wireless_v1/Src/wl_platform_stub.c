/**
 * @file    wl_platform_stub.c
 * @brief   平台抽象层桩实现（用于离线测试 / PC 端编译）。
 *
 * 当未定义 WL_USE_STM32F1_HAL_PORT 时编译此文件，
 * 提供 wl_platform.h 中所有接口的空实现或简单模拟。
 */

#ifndef WL_USE_STM32F1_HAL_PORT

#include "wl_platform.h"
#include "wl_config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  模拟毫秒计数器                                                     */
/* ------------------------------------------------------------------ */

static uint32_t stub_millis = 0;

/* ------------------------------------------------------------------ */
/*  模拟 UART 接收缓冲区                                               */
/* ------------------------------------------------------------------ */

static uint8_t  stub_rx_buf[WL_UART_RX_BUF_SIZE];
static uint16_t stub_rx_len = 0;
static uint16_t stub_rx_pos = 0;

/* ------------------------------------------------------------------ */
/*  PAL 接口桩实现                                                     */
/* ------------------------------------------------------------------ */

void WL_Platform_Init(void)
{
    stub_millis = 0;
    stub_rx_len = 0;
    stub_rx_pos = 0;
    printf("[桩] WL_Platform_Init 完成\n");
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
    printf("[桩] UART 发送 %u 字节: ", len);
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] >= 0x20 && data[i] < 0x7F) {
            putchar(data[i]);
        } else {
            printf("\\x%02X", data[i]);
        }
    }
    printf("\n");

    /* 模拟 AT 指令响应：如果发送的是 "AT" 开头，自动填充 "AT_OK\r\n" */
    if (len >= 2 && data[0] == 'A' && data[1] == 'T') {
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
    printf("[调试] %s", msg);
}

/* ------------------------------------------------------------------ */
/*  桩专用辅助函数（供测试代码调用）                                     */
/* ------------------------------------------------------------------ */

/** 手动推进模拟时钟（毫秒）。 */
void WL_Stub_AdvanceMillis(uint32_t ms)
{
    stub_millis += ms;
}

/** 向模拟 UART 接收缓冲区注入数据（模拟模块返回的响应）。 */
void WL_Stub_InjectRxData(const uint8_t *data, uint16_t len)
{
    if (len > WL_UART_RX_BUF_SIZE) {
        len = WL_UART_RX_BUF_SIZE;
    }
    memcpy(stub_rx_buf, data, len);
    stub_rx_len = len;
    stub_rx_pos = 0;
}

#endif /* !WL_USE_STM32F1_HAL_PORT */
