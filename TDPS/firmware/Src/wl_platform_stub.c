/**
 * @file    wl_platform_stub.c
 * @brief   Platform abstraction stub implementation (for offline testing / PC builds).
 *
 * When neither WL_USE_STM32F4_HAL_PORT nor WL_USE_POSIX_SERIAL_PORT is
 * defined, this file is compiled, providing empty implementations or simple
 * simulations for all interfaces in wl_platform.h.
 */

#if !defined(WL_USE_STM32F4_HAL_PORT) && !defined(WL_USE_POSIX_SERIAL_PORT)

#include "wl_config.h"
#include "wl_platform.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Simulated millisecond counter                                      */
/* ------------------------------------------------------------------ */

static uint32_t stub_millis = 0;

/* ------------------------------------------------------------------ */
/*  Simulated UART receive buffer                                      */
/* ------------------------------------------------------------------ */

static uint8_t stub_rx_buf[WL_UART_RX_BUF_SIZE];
static uint16_t stub_rx_len = 0;
static uint16_t stub_rx_pos = 0;

/* Whether to auto-inject AT_OK responses. */
static bool stub_auto_at_response = true;

/* AUX pin simulated state. */
static bool stub_aux_ready = true;

/* ------------------------------------------------------------------ */
/*  Last UART transmit cache (for automated test assertions)           */
/* ------------------------------------------------------------------ */

static uint8_t stub_last_tx_buf[WL_UART_TX_BUF_SIZE];
static uint16_t stub_last_tx_len = 0;
static uint32_t stub_tx_count = 0U;

static void stub_compact_rx(void)
{
    if (stub_rx_pos == 0U) {
        return;
    }

    if (stub_rx_pos >= stub_rx_len) {
        stub_rx_len = 0U;
        stub_rx_pos = 0U;
        return;
    }

    memmove(stub_rx_buf, &stub_rx_buf[stub_rx_pos], (size_t)(stub_rx_len - stub_rx_pos));
    stub_rx_len = (uint16_t)(stub_rx_len - stub_rx_pos);
    stub_rx_pos = 0U;
}

/* ------------------------------------------------------------------ */
/*  PAL interface stub implementations                                 */
/* ------------------------------------------------------------------ */

void WL_Platform_Init(void)
{
    stub_millis = 0;
    stub_rx_len = 0;
    stub_rx_pos = 0;
    stub_auto_at_response = true;
    stub_aux_ready = true;
    stub_last_tx_len = 0;
    stub_tx_count = 0U;
#ifndef WL_STUB_QUIET
    printf("[Stub] WL_Platform_Init done\n");
#endif
}

uint32_t WL_Platform_GetMillis(void)
{
#ifdef WL_STUB_USE_LF_TIME
    extern uint32_t LF_Platform_GetMillis(void);
    return LF_Platform_GetMillis();
#else
    return stub_millis;
#endif
}

void WL_Platform_DelayMs(uint32_t ms)
{
#ifdef WL_STUB_USE_LF_TIME
    extern void LF_Platform_DelayMs(uint32_t ms);
    LF_Platform_DelayMs(ms);
#else
    stub_millis += ms;
#endif
}

void WL_Platform_UART_Send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U) {
        return;
    }

    stub_tx_count += 1U;
    stub_last_tx_len = len;
    if (stub_last_tx_len > WL_UART_TX_BUF_SIZE) {
        stub_last_tx_len = WL_UART_TX_BUF_SIZE;
    }
    if (stub_last_tx_len > 0U) {
        memcpy(stub_last_tx_buf, data, stub_last_tx_len);
    }

#ifndef WL_STUB_QUIET
    printf("[Stub] UART TX %u bytes: ", len);
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] >= 0x20 && data[i] < 0x7F) {
            putchar(data[i]);
        } else {
            printf("\\x%02X", data[i]);
        }
    }
    printf("\n");
#endif

    /* Simulate AT command response: if the data starts with "AT",
     * automatically inject "AT_OK\r\n" */
    if (stub_auto_at_response && len >= 2U && data[0] == 'A' && data[1] == 'T') {
        const char *resp = "AT_OK\r\n";
        uint16_t rlen = (uint16_t)strlen(resp);
        stub_compact_rx();
        if ((uint16_t)(stub_rx_len + rlen) <= WL_UART_RX_BUF_SIZE) {
            memcpy(&stub_rx_buf[stub_rx_len], resp, rlen);
            stub_rx_len = (uint16_t)(stub_rx_len + rlen);
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
    return stub_aux_ready;
}

void WL_Platform_ResetModule(void)
{
}

void WL_Platform_DebugPrint(const char *msg)
{
#ifndef WL_STUB_QUIET
    printf("[Debug] %s", msg);
#else
    (void)msg;
#endif
}

/* ------------------------------------------------------------------ */
/*  Stub-specific helper functions (called by test code)               */
/* ------------------------------------------------------------------ */

void WL_Stub_AdvanceMillis(uint32_t ms)
{
    stub_millis += ms;
}

void WL_Stub_InjectRxData(const uint8_t *data, uint16_t len)
{
    if (data == NULL) {
        stub_rx_len = 0U;
        stub_rx_pos = 0U;
        return;
    }

    if (len > WL_UART_RX_BUF_SIZE) {
        len = WL_UART_RX_BUF_SIZE;
    }

    memcpy(stub_rx_buf, data, len);
    stub_rx_len = len;
    stub_rx_pos = 0;
}

void WL_Stub_AppendRxData(const uint8_t *data, uint16_t len)
{
    uint16_t append_len;

    if (data == NULL || len == 0U) {
        return;
    }

    stub_compact_rx();
    if (stub_rx_len >= WL_UART_RX_BUF_SIZE) {
        return;
    }

    append_len = len;
    if ((uint16_t)(stub_rx_len + append_len) > WL_UART_RX_BUF_SIZE) {
        append_len = (uint16_t)(WL_UART_RX_BUF_SIZE - stub_rx_len);
    }

    if (append_len > 0U) {
        memcpy(&stub_rx_buf[stub_rx_len], data, append_len);
        stub_rx_len = (uint16_t)(stub_rx_len + append_len);
    }
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

void WL_Stub_ResetTxCount(void)
{
    stub_tx_count = 0U;
}

uint32_t WL_Stub_GetTxCount(void)
{
    return stub_tx_count;
}

void WL_Stub_SetAuxReady(bool ready)
{
    stub_aux_ready = ready;
}

bool WL_Stub_GetAuxReady(void)
{
    return stub_aux_ready;
}

void WL_Stub_SetAutoAtResponse(bool enable)
{
    stub_auto_at_response = enable;
}

#endif /* !WL_USE_STM32F4_HAL_PORT && !WL_USE_POSIX_SERIAL_PORT */
