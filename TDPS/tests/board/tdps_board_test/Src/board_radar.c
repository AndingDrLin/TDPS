#include "board_radar.h"
#include "board_cli.h"
#include <string.h>

#define RADAR_BUF_SIZE 96U
#define RADAR_RAW_BUF_SIZE 128U

static volatile uint32_t s_rx_count;
static volatile uint32_t s_error_count;
static uint8_t s_raw_enabled;
static uint8_t s_parse_enabled;
static uint8_t s_buf[RADAR_BUF_SIZE];
static uint8_t s_len;
static uint8_t s_target_state;
static uint16_t s_distance_cm;
static uint32_t s_last_frame_tick;
static volatile uint8_t s_raw_buf[RADAR_RAW_BUF_SIZE];
static volatile uint16_t s_raw_write;
static volatile uint16_t s_raw_read;

static void reset_parser(void)
{
    s_len = 0;
}

static void raw_push(uint8_t byte)
{
    uint16_t next = (uint16_t)((s_raw_write + 1U) % RADAR_RAW_BUF_SIZE);
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
    s_raw_read = (uint16_t)((s_raw_read + 1U) % RADAR_RAW_BUF_SIZE);
    return 1;
}

static void parse_byte(uint8_t byte)
{
    if (s_len == 0U)
    {
        if ((byte != 0x6E) && (byte != 0xF4))
        {
            return;
        }
    }

    if (s_len < RADAR_BUF_SIZE)
    {
        s_buf[s_len++] = byte;
    }
    else
    {
        reset_parser();
        return;
    }

    if ((s_buf[0] == 0x6E) && (s_len == 5U))
    {
        if (s_buf[4] == 0x62)
        {
            s_target_state = s_buf[1];
            s_distance_cm = (uint16_t)(s_buf[2] | ((uint16_t)s_buf[3] << 8));
            s_last_frame_tick = HAL_GetTick();
        }
        reset_parser();
    }
    else if ((s_buf[0] == 0xF4) && (s_len == 70U))
    {
        if ((s_buf[1] == 0xF3) && (s_buf[2] == 0xF2) && (s_buf[3] == 0xF1) &&
            (s_buf[66] == 0xF8) && (s_buf[67] == 0xF7) && (s_buf[68] == 0xF6) && (s_buf[69] == 0xF5))
        {
            s_target_state = s_buf[7];
            s_distance_cm = (uint16_t)(s_buf[8] | ((uint16_t)s_buf[9] << 8));
            s_last_frame_tick = HAL_GetTick();
        }
        reset_parser();
    }
}

void BoardRadar_Init(void)
{
    s_rx_count = 0;
    s_error_count = 0;
    s_raw_enabled = 0;
    s_parse_enabled = 1;
    s_target_state = 0;
    s_distance_cm = 0;
    s_last_frame_tick = 0;
    s_raw_write = 0;
    s_raw_read = 0;
    reset_parser();
}

void BoardRadar_Task(void)
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

void BoardRadar_OnRxByte(uint8_t byte)
{
    s_rx_count++;
    if (s_raw_enabled)
    {
        raw_push(byte);
    }
    if (s_parse_enabled)
    {
        parse_byte(byte);
    }
}

void BoardRadar_OnError(void)
{
    s_error_count++;
    reset_parser();
}

void BoardRadar_SetRaw(uint8_t enabled)
{
    s_raw_enabled = enabled ? 1U : 0U;
    s_raw_write = 0;
    s_raw_read = 0;
    Board_Printf("RADAR raw %s\r\n", s_raw_enabled ? "on" : "off");
}

void BoardRadar_SetParse(uint8_t enabled)
{
    s_parse_enabled = enabled ? 1U : 0U;
    reset_parser();
    Board_Printf("RADAR parse %s\r\n", s_parse_enabled ? "on" : "off");
}

void BoardRadar_PrintStatus(void)
{
    Board_Printf("RADAR UART=USART3 PB10/PB11 RX=%lu ERR=%lu GPIO_PE15=%u target=%u distance_cm=%u last_ms=%lu RAW=%u PARSE=%u\r\n",
                 (unsigned long)s_rx_count,
                 (unsigned long)s_error_count,
                 (unsigned)HAL_GPIO_ReadPin(RADAR_GPIO_GPIO_Port, RADAR_GPIO_Pin),
                 (unsigned)s_target_state,
                 (unsigned)s_distance_cm,
                 (unsigned long)s_last_frame_tick,
                 (unsigned)s_raw_enabled,
                 (unsigned)s_parse_enabled);
}

void BoardRadar_PrintGpio(void)
{
    Board_Printf("RADAR GPIO PE15=%u\r\n", (unsigned)HAL_GPIO_ReadPin(RADAR_GPIO_GPIO_Port, RADAR_GPIO_Pin));
}

uint32_t BoardRadar_GetRxCount(void)
{
    return s_rx_count;
}

uint32_t BoardRadar_GetErrorCount(void)
{
    return s_error_count;
}

uint8_t BoardRadar_GetTargetState(void)
{
    return s_target_state;
}

uint16_t BoardRadar_GetDistanceCm(void)
{
    return s_distance_cm;
}

uint32_t BoardRadar_GetLastFrameTick(void)
{
    return s_last_frame_tick;
}
