#include "board_line.h"
#include "board_cli.h"
#include <string.h>

#define LINE_RAW_BUF_SIZE 128U

extern UART_HandleTypeDef huart2;

static volatile uint32_t s_rx_count;
static volatile uint32_t s_error_count;
static uint8_t s_raw_enabled;
static uint32_t s_last_rx_tick;
static volatile uint8_t s_raw_buf[LINE_RAW_BUF_SIZE];
static volatile uint16_t s_raw_write;
static volatile uint16_t s_raw_read;

static void raw_push(uint8_t byte)
{
    uint16_t next = (uint16_t)((s_raw_write + 1U) % LINE_RAW_BUF_SIZE);
    if (next != s_raw_read)
    {
        s_raw_buf[s_raw_write] = byte;
        s_raw_write = next;
    }
}

static uint8_t raw_pop(uint8_t *byte)
{
    if (s_raw_read == s_raw_write)
    {
        return 0;
    }
    *byte = s_raw_buf[s_raw_read];
    s_raw_read = (uint16_t)((s_raw_read + 1U) % LINE_RAW_BUF_SIZE);
    return 1;
}

void BoardLine_Init(void)
{
    s_rx_count = 0;
    s_error_count = 0;
    s_raw_enabled = 0;
    s_last_rx_tick = 0;
    s_raw_write = 0;
    s_raw_read = 0;
}

void BoardLine_Task(void)
{
    uint8_t byte;
    uint8_t printed = 0;

    if (!s_raw_enabled)
    {
        return;
    }

    while (printed < 16U && raw_pop(&byte))
    {
        Board_Printf("%02X ", byte);
        printed++;
    }
}

void BoardLine_OnRxByte(uint8_t byte)
{
    s_rx_count++;
    s_last_rx_tick = HAL_GetTick();
    if (s_raw_enabled)
    {
        raw_push(byte);
    }
}

void BoardLine_OnError(void)
{
    s_error_count++;
}

void BoardLine_PrintStatus(void)
{
    Board_Printf("LINE UART=USART2 PA2/PA3 RX=%lu ERR=%lu last_ms=%lu RAW=%u SIG1=%u SIG2=%u\r\n",
                 (unsigned long)s_rx_count,
                 (unsigned long)s_error_count,
                 (unsigned long)s_last_rx_tick,
                 (unsigned)s_raw_enabled,
                 (unsigned)HAL_GPIO_ReadPin(LINE2_SIG1_GPIO_Port, LINE2_SIG1_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LINE2_SIG2_GPIO_Port, LINE2_SIG2_Pin));
}

void BoardLine_PrintIo(void)
{
    Board_Printf("LINE IO PB0_SIG1=%u PB1_SIG2=%u\r\n",
                 (unsigned)HAL_GPIO_ReadPin(LINE2_SIG1_GPIO_Port, LINE2_SIG1_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LINE2_SIG2_GPIO_Port, LINE2_SIG2_Pin));
}

void BoardLine_SetRaw(uint8_t enabled)
{
    s_raw_enabled = enabled ? 1U : 0U;
    s_raw_write = 0;
    s_raw_read = 0;
    Board_Printf("LINE raw %s\r\n", s_raw_enabled ? "on" : "off");
}

HAL_StatusTypeDef BoardLine_SendString(const char *text)
{
    size_t len;

    if (text == 0)
    {
        return HAL_ERROR;
    }

    len = strlen(text);
    if (len == 0U)
    {
        return HAL_OK;
    }

    return HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)len, 100);
}

uint32_t BoardLine_GetRxCount(void)
{
    return s_rx_count;
}

uint32_t BoardLine_GetErrorCount(void)
{
    return s_error_count;
}

uint32_t BoardLine_GetLastRxTick(void)
{
    return s_last_rx_tick;
}
