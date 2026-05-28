#include "lf_sensor_uart.h"

#include <string.h>

#include "lf_port_stm32f4_hal.h"

typedef enum {
    UART_PARSE_IDLE = 0,
    UART_PARSE_DATA,
    UART_PARSE_CHECKSUM,
} UART_ParseState;

static UART_HandleTypeDef *s_huart;
static volatile UART_ParseState s_parse_state;
static volatile uint8_t s_parse_buf[9];
static volatile uint8_t s_parse_idx;
static volatile uint8_t s_parse_sum;

static volatile uint8_t s_latest_values[LF_SENSOR_COUNT];
static volatile bool s_frame_valid;
static volatile uint32_t s_valid_count;
static volatile uint32_t s_error_count;

void LF_SensorUart_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_parse_state = UART_PARSE_IDLE;
    s_parse_idx = 0U;
    s_parse_sum = 0U;
    s_frame_valid = false;
    s_valid_count = 0U;
    s_error_count = 0U;
    memset((void *)s_latest_values, 0, sizeof(s_latest_values));
}

void LF_SensorUart_OnByte(UART_HandleTypeDef *huart, uint8_t byte)
{
    if (huart != s_huart) {
        return;
    }

    switch (s_parse_state) {
    case UART_PARSE_IDLE:
        if (byte == LF_SENSOR_UART_HEADER) {
            s_parse_state = UART_PARSE_DATA;
            s_parse_idx = 0U;
            s_parse_sum = byte;
        }
        break;

    case UART_PARSE_DATA:
        s_parse_buf[s_parse_idx] = byte;
        s_parse_sum += byte;
        s_parse_idx++;
        if (s_parse_idx >= 9U) {
            s_parse_state = UART_PARSE_CHECKSUM;
        }
        break;

    case UART_PARSE_CHECKSUM:
        if (byte == (s_parse_sum & 0xFFU)) {
            memcpy((void *)s_latest_values, (const void *)s_parse_buf, LF_SENSOR_COUNT);
            s_frame_valid = true;
            s_valid_count++;
        } else {
            s_error_count++;
        }
        s_parse_state = UART_PARSE_IDLE;
        break;
    }
}

bool LF_SensorUart_GetFrame(uint8_t out_values[LF_SENSOR_COUNT])
{
    if (out_values == NULL || !s_frame_valid) {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    memcpy(out_values, (const void *)s_latest_values, LF_SENSOR_COUNT);
    __set_PRIMASK(primask);

    return true;
}

void LF_SensorUart_GetFrontAux(bool *out_left, bool *out_right)
{
    if (out_left != NULL) {
        *out_left = (HAL_GPIO_ReadPin(LF_PORT_FRONT_AUX_LEFT_PORT,
                                       LF_PORT_FRONT_AUX_LEFT_PIN) == GPIO_PIN_SET);
    }
    if (out_right != NULL) {
        *out_right = (HAL_GPIO_ReadPin(LF_PORT_FRONT_AUX_RIGHT_PORT,
                                        LF_PORT_FRONT_AUX_RIGHT_PIN) == GPIO_PIN_SET);
    }
}

uint32_t LF_SensorUart_GetValidFrameCount(void)
{
    return s_valid_count;
}

uint32_t LF_SensorUart_GetErrorCount(void)
{
    return s_error_count;
}
