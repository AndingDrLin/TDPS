#include "ld2410s.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define LD2410S_RX_BUF_SIZE 512

static UART_HandleTypeDef *s_radar_huart = NULL;
static UART_HandleTypeDef *s_debug_huart = NULL;

static uint8_t s_radar_rx_byte = 0;
static volatile uint8_t s_rx_buffer[LD2410S_RX_BUF_SIZE];
static volatile uint16_t s_rx_write = 0;
static volatile uint16_t s_rx_read = 0;
static volatile uint32_t s_raw_rx_bytes = 0;
static volatile uint32_t s_rx_overflow_count = 0;
static volatile uint32_t s_parse_error_count = 0;
static volatile uint32_t s_frame_count = 0;
static HAL_StatusTypeDef s_last_rx_status = HAL_OK;

static const uint8_t s_cmd_enable_cfg[] =
{
    0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00,
    0x01, 0x00, 0x04, 0x03, 0x02, 0x01
};

static const uint8_t s_cmd_switch_std[] =
{
    0xFD, 0xFC, 0xFB, 0xFA, 0x08, 0x00, 0x7A, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x03,
    0x02, 0x01
};

static const uint8_t s_cmd_end_cfg[] =
{
    0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00,
    0x04, 0x03, 0x02, 0x01
};

static uint8_t pop_byte(uint8_t *byte)
{
    if (s_rx_read == s_rx_write)
    {
        return 0;
    }

    *byte = s_rx_buffer[s_rx_read];
    s_rx_read = (uint16_t)((s_rx_read + 1U) % LD2410S_RX_BUF_SIZE);
    return 1;
}

static uint32_t u32_from_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void Debug_Printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    int len;

    if (s_debug_huart == NULL)
    {
        return;
    }

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0)
    {
        if (len > (int)sizeof(buf))
        {
            len = sizeof(buf);
        }
        HAL_UART_Transmit(s_debug_huart, (uint8_t *)buf, (uint16_t)len, 20);
    }
}

void LD2410S_Init(UART_HandleTypeDef *radar_huart, UART_HandleTypeDef *debug_huart)
{
    s_radar_huart = radar_huart;
    s_debug_huart = debug_huart;
    s_rx_write = 0;
    s_rx_read = 0;
    s_raw_rx_bytes = 0;
    s_rx_overflow_count = 0;
    s_parse_error_count = 0;
    s_frame_count = 0;
    s_last_rx_status = HAL_OK;
}

void LD2410S_StartReceiveIT(void)
{
    if (s_radar_huart == NULL)
    {
        s_last_rx_status = HAL_ERROR;
        return;
    }

    s_last_rx_status = HAL_UART_Receive_IT(s_radar_huart, &s_radar_rx_byte, 1);
    if (s_last_rx_status != HAL_OK)
    {
        (void)HAL_UART_AbortReceive_IT(s_radar_huart);
        s_last_rx_status = HAL_UART_Receive_IT(s_radar_huart, &s_radar_rx_byte, 1);
    }
}

void LD2410S_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t next;

    if (huart != s_radar_huart)
    {
        return;
    }

    s_raw_rx_bytes++;
    next = (uint16_t)((s_rx_write + 1U) % LD2410S_RX_BUF_SIZE);
    if (next != s_rx_read)
    {
        s_rx_buffer[s_rx_write] = s_radar_rx_byte;
        s_rx_write = next;
    }
    else
    {
        s_rx_overflow_count++;
    }

    LD2410S_StartReceiveIT();
}

void LD2410S_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != s_radar_huart)
    {
        return;
    }

    __HAL_UART_CLEAR_OREFLAG(huart);
    LD2410S_StartReceiveIT();
}

uint32_t LD2410S_FrameCount(void)
{
    return s_frame_count;
}

uint32_t LD2410S_RawRxBytes(void)
{
    return s_raw_rx_bytes;
}

uint32_t LD2410S_ParseErrorCount(void)
{
    return s_parse_error_count;
}

uint32_t LD2410S_RxOverflowCount(void)
{
    return s_rx_overflow_count;
}

HAL_StatusTypeDef LD2410S_LastRxStatus(void)
{
    return s_last_rx_status;
}

void LD2410S_SwitchToStandardMode(void)
{
    Debug_Printf("Switching LD2410S to standard mode...\r\n");
    HAL_UART_Transmit(s_radar_huart, (uint8_t *)s_cmd_enable_cfg, sizeof(s_cmd_enable_cfg), 100);
    HAL_Delay(50);
    HAL_UART_Transmit(s_radar_huart, (uint8_t *)s_cmd_switch_std, sizeof(s_cmd_switch_std), 100);
    HAL_Delay(50);
    HAL_UART_Transmit(s_radar_huart, (uint8_t *)s_cmd_end_cfg, sizeof(s_cmd_end_cfg), 100);
    HAL_Delay(50);
}

const char *LD2410S_TargetStateString(uint8_t state)
{
    switch (state)
    {
    case 0: return "no_target";
    case 1: return "clear";
    case 2: return "target_present";
    case 3: return "target_present_strong";
    default: return "unknown";
    }
}

const char *LD2410S_GateRangeString(uint8_t gate)
{
    static const char *table[16] =
    {
        "0m", "0.0-0.7m", "0.7-1.4m", "1.4-2.1m",
        "2.1-2.8m", "2.8-3.5m", "3.5-4.2m", "4.2-4.9m",
        "4.9-5.6m", "5.6-6.3m", "6.3-7.0m", "7.0-7.7m",
        "7.7-8.4m", "8.4-9.1m", "9.1-9.8m", "9.8-10.5m"
    };

    if (gate < 16U)
    {
        return table[gate];
    }

    return "invalid";
}

uint8_t LD2410S_GetFrame(LD2410S_FrameInfo *info)
{
    static uint8_t frame_buf[80];
    static uint8_t frame_len = 0;
    uint8_t byte;
    uint8_t i;

    while (pop_byte(&byte))
    {
        if (frame_len == 0U)
        {
            if ((byte == 0x6E) || (byte == 0xF4))
            {
                frame_buf[frame_len++] = byte;
            }
            continue;
        }

        if (frame_buf[0] == 0xF4)
        {
            static const uint8_t f4_header[4] = {0xF4, 0xF3, 0xF2, 0xF1};
            if (frame_len < 4U && byte != f4_header[frame_len])
            {
                s_parse_error_count++;
                frame_len = 0;
                if (byte == 0x6E)
                {
                    frame_buf[frame_len++] = byte;
                }
                else if (byte == 0xF4)
                {
                    frame_buf[frame_len++] = byte;
                }
                continue;
            }
        }

        if (frame_len < sizeof(frame_buf))
        {
            frame_buf[frame_len++] = byte;
        }
        else
        {
            s_parse_error_count++;
            frame_len = 0;
            continue;
        }

        if ((frame_buf[0] == 0x6E) && (frame_len == 5U))
        {
            if (frame_buf[4] == 0x62)
            {
                memset(info, 0, sizeof(LD2410S_FrameInfo));
                info->frame_type = LD2410S_FRAME_MINIMAL;
                info->target_state = frame_buf[1];
                info->distance_cm = (uint16_t)(frame_buf[2] | ((uint16_t)frame_buf[3] << 8));
                s_frame_count++;
                frame_len = 0;
                return 1;
            }
            s_parse_error_count++;
            frame_len = 0;
        }
        else if ((frame_buf[0] == 0xF4) && (frame_len == 70U))
        {
            if ((frame_buf[1] == 0xF3) &&
                (frame_buf[2] == 0xF2) &&
                (frame_buf[3] == 0xF1) &&
                (frame_buf[4] == 0x46) &&
                (frame_buf[5] == 0x00) &&
                (frame_buf[6] == 0x01) &&
                (frame_buf[66] == 0xF8) &&
                (frame_buf[67] == 0xF7) &&
                (frame_buf[68] == 0xF6) &&
                (frame_buf[69] == 0xF5))
            {
                uint32_t max_energy = 0;

                memset(info, 0, sizeof(LD2410S_FrameInfo));
                info->frame_type = LD2410S_FRAME_STANDARD;
                info->target_state = frame_buf[7];
                info->distance_cm = (uint16_t)(frame_buf[8] | ((uint16_t)frame_buf[9] << 8));

                for (i = 0; i < 16U; i++)
                {
                    uint8_t offset = (uint8_t)(12U + i * 4U);
                    info->gate_energy[i] = u32_from_le(&frame_buf[offset]);
                    if (info->gate_energy[i] >= max_energy)
                    {
                        max_energy = info->gate_energy[i];
                        info->strongest_gate = i;
                    }
                }

                s_frame_count++;
                frame_len = 0;
                return 1;
            }
            s_parse_error_count++;
            frame_len = 0;
        }
    }

    return 0;
}