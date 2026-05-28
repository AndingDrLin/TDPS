#include "board_lora.h"
#include "board_cli.h"
#include "board_pins.h"
#include <string.h>

#define LORA_RAW_BUF_SIZE 128U

extern UART_HandleTypeDef huart5;

static volatile uint32_t s_rx_count;
static volatile uint32_t s_error_count;
static uint8_t s_raw_enabled;
static volatile uint8_t s_raw_buf[LORA_RAW_BUF_SIZE];
static volatile uint16_t s_raw_write;
static volatile uint16_t s_raw_read;

static void raw_push(uint8_t byte)
{
    uint16_t next = (uint16_t)((s_raw_write + 1U) % LORA_RAW_BUF_SIZE);
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
    s_raw_read = (uint16_t)((s_raw_read + 1U) % LORA_RAW_BUF_SIZE);
    return 1;
}

void BoardLora_Init(void)
{
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, BOARD_LORA_RST_IDLE_STATE);
    HAL_GPIO_WritePin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin, GPIO_PIN_RESET);
    s_rx_count = 0;
    s_error_count = 0;
    s_raw_enabled = 0;
    s_raw_write = 0;
    s_raw_read = 0;
}

void BoardLora_Task(void)
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

void BoardLora_OnRxByte(uint8_t byte)
{
    s_rx_count++;
    if (s_raw_enabled)
    {
        raw_push(byte);
    }
}

void BoardLora_OnError(void)
{
    s_error_count++;
}

void BoardLora_PrintStatus(void)
{
    Board_Printf("LORA UART=UART5 PC12/PD2 RX=%lu ERR=%lu AUX=%u LINK=%u WAKE=%u RST=%u RAW=%u\r\n",
                 (unsigned long)s_rx_count,
                 (unsigned long)s_error_count,
                 (unsigned)HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LORA_LINK_GPIO_Port, LORA_LINK_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LORA_RST_GPIO_Port, LORA_RST_Pin),
                 (unsigned)s_raw_enabled);
}

void BoardLora_Reset(void)
{
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, BOARD_LORA_RST_ACTIVE_STATE);
    HAL_Delay(50);
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, BOARD_LORA_RST_IDLE_STATE);
    HAL_Delay(100);
    Board_Printf("LORA reset pulse done\r\n");
}

void BoardLora_SetWake(uint8_t level)
{
    HAL_GPIO_WritePin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    Board_Printf("LORA WAKE=%u\r\n", (unsigned)level);
}

void BoardLora_SetRaw(uint8_t enabled)
{
    s_raw_enabled = enabled ? 1U : 0U;
    s_raw_write = 0;
    s_raw_read = 0;
    Board_Printf("LORA raw %s\r\n", s_raw_enabled ? "on" : "off");
}

HAL_StatusTypeDef BoardLora_SendString(const char *text)
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

    return HAL_UART_Transmit(&huart5, (uint8_t *)text, (uint16_t)len, 100);
}

uint32_t BoardLora_GetRxCount(void)
{
    return s_rx_count;
}

uint32_t BoardLora_GetErrorCount(void)
{
    return s_error_count;
}
