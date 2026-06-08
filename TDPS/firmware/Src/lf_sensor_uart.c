/**
 * @file lf_sensor_uart.c
 * @brief UART-based line sensor frame parser (Yahboom 8-LP protocol).
 */

#include "lf_sensor_uart.h"

#include <stddef.h>
#include <string.h>

static UART_HandleTypeDef *s_huart;
static volatile char s_rx_frame[LF_SENSOR_UART_MAX_FRAME_LEN];
static volatile uint16_t s_rx_index;
static volatile uint8_t s_collecting;
static volatile uint16_t s_analog_raw[LF_SENSOR_COUNT];
static volatile uint8_t s_digital_level[LF_SENSOR_COUNT];
static volatile char s_last_ascii_frame[LF_SENSOR_UART_MAX_FRAME_LEN];
static volatile LF_SensorUartStats s_stats;

static uint8_t is_digit_char(char c)
{
    return (c >= '0' && c <= '9') ? 1U : 0U;
}

static uint8_t parse_uint16(const char **cursor, uint16_t max_value, uint16_t *out_value)
{
    uint32_t value = 0U;
    uint8_t digits = 0U;

    while (is_digit_char(**cursor) && digits < 4U) {
        value = (value * 10U) + (uint32_t)(**cursor - '0');
        if (value > max_value) {
            value = max_value;
        }
        (*cursor)++;
        digits++;
    }

    if (digits == 0U || out_value == NULL) {
        return 0U;
    }

    *out_value = (uint16_t)value;
    return 1U;
}

static uint8_t parse_values(const char *frame, char frame_type, uint16_t out_values[LF_SENSOR_COUNT])
{
    uint32_t i;
    const char *p = frame;
    uint16_t max_value = (frame_type == 'D') ? 1U : 4095U;

    if (frame == NULL || out_values == NULL || p[0] != '$' || p[1] != frame_type) {
        return 0U;
    }

    p += 2;
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        uint16_t value = 0U;
        char expected_index = (char)('1' + (char)i);

        if (*p != ',') {
            return 0U;
        }
        p++;
        if (*p != 'x' && *p != 'X') {
            return 0U;
        }
        p++;
        if (*p != expected_index) {
            return 0U;
        }
        p++;
        if (*p != ':') {
            return 0U;
        }
        p++;
        if (!parse_uint16(&p, max_value, &value)) {
            return 0U;
        }
        out_values[i] = value;
    }

    return (*p == '#') ? 1U : 0U;
}

static void store_last_frame(const char *frame)
{
    uint32_t i;

    for (i = 0U; i < (LF_SENSOR_UART_MAX_FRAME_LEN - 1U) && frame[i] != '\0'; ++i) {
        s_last_ascii_frame[i] = frame[i];
    }
    s_last_ascii_frame[i] = '\0';
}

static void process_frame(const char *frame)
{
    uint16_t values[LF_SENSOR_COUNT];
    uint32_t i;

    if (parse_values(frame, 'A', values)) {
        for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
            s_analog_raw[i] = values[i];
        }
        s_stats.analog_frame_valid = 1U;
        s_stats.analog_frame_count++;
        s_stats.last_frame_ms = HAL_GetTick();
        store_last_frame(frame);
        return;
    }

    if (parse_values(frame, 'D', values)) {
        for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
            s_digital_level[i] = (uint8_t)(values[i] != 0U ? 1U : 0U);
        }
        s_stats.digital_frame_valid = 1U;
        s_stats.digital_frame_count++;
        s_stats.last_frame_ms = HAL_GetTick();
        store_last_frame(frame);
        return;
    }

    s_stats.parse_error_count++;
}

void LF_SensorUart_Init(UART_HandleTypeDef *huart)
{
    uint32_t i;

    s_huart = huart;
    s_rx_index = 0U;
    s_collecting = 0U;
    memset((void *)&s_stats, 0, sizeof(s_stats));

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        s_analog_raw[i] = 0U;
        s_digital_level[i] = 0U;
    }
    for (i = 0U; i < LF_SENSOR_UART_MAX_FRAME_LEN; ++i) {
        s_rx_frame[i] = '\0';
        s_last_ascii_frame[i] = '\0';
    }
}

void LF_SensorUart_OnByte(UART_HandleTypeDef *huart, uint8_t byte)
{
    char frame[LF_SENSOR_UART_MAX_FRAME_LEN];
    uint32_t i;

    if (huart != s_huart) {
        return;
    }

    s_stats.rx_byte_count++;
    s_stats.last_rx_byte = byte;

    if (byte == '$') {
        s_collecting = 1U;
        s_rx_index = 0U;
        s_rx_frame[s_rx_index++] = (char)byte;
        return;
    }

    if (s_collecting == 0U) {
        return;
    }

    if (s_rx_index >= (LF_SENSOR_UART_MAX_FRAME_LEN - 1U)) {
        s_collecting = 0U;
        s_rx_index = 0U;
        s_stats.parse_error_count++;
        return;
    }

    s_rx_frame[s_rx_index++] = (char)byte;

    if (byte != '#') {
        return;
    }

    s_rx_frame[s_rx_index] = '\0';
    for (i = 0U; i < s_rx_index && i < (LF_SENSOR_UART_MAX_FRAME_LEN - 1U); ++i) {
        frame[i] = (char)s_rx_frame[i];
    }
    frame[i] = '\0';

    s_collecting = 0U;
    s_rx_index = 0U;
    process_frame(frame);
}

void LF_SensorUart_RecordUartError(void)
{
    s_stats.uart_error_count++;
}

/*
 * Interrupt-driven transmit mode: use HAL_UART_Transmit_IT instead of
 * blocking transmit. s_tx_busy is true while transmitting and cleared in
 * the completion callback.
 * If the previous frame is still being transmitted, skip the current command
 * (sensor commands are polled periodically; missing one frame is harmless).
 */
static volatile bool s_tx_busy = false;

HAL_StatusTypeDef LF_SensorUart_SendCommand(const char *cmd)
{
    if (s_huart == NULL || cmd == NULL) {
        return HAL_ERROR;
    }
    if (s_tx_busy) {
        return HAL_BUSY;
    }

    s_tx_busy = true;
    HAL_StatusTypeDef status = HAL_UART_Transmit_IT(s_huart, (uint8_t *)cmd, (uint16_t)strlen(cmd));
    if (status != HAL_OK) {
        s_tx_busy = false;
    }
    return status;
}

/* Called by HAL_UART_TxCpltCallback to clear the transmit busy flag. */
void LF_SensorUart_OnTxComplete(void)
{
    s_tx_busy = false;
}

bool LF_SensorUart_GetAnalogFrame(uint16_t out_values[LF_SENSOR_COUNT])
{
    uint32_t primask;
    uint32_t i;

    if (out_values == NULL || s_stats.analog_frame_valid == 0U) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        out_values[i] = s_analog_raw[i];
    }
    __set_PRIMASK(primask);
    return true;
}

bool LF_SensorUart_GetDigitalFrame(uint8_t out_values[LF_SENSOR_COUNT])
{
    uint32_t primask;
    uint32_t i;

    if (out_values == NULL || s_stats.digital_frame_valid == 0U) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        out_values[i] = s_digital_level[i];
    }
    __set_PRIMASK(primask);
    return true;
}

bool LF_SensorUart_GetFrame(uint8_t out_values[LF_SENSOR_COUNT])
{
    uint16_t analog[LF_SENSOR_COUNT];
    uint32_t i;

    if (out_values == NULL || !LF_SensorUart_GetAnalogFrame(analog)) {
        return false;
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        out_values[i] = (analog[i] >= 4080U) ? 255U : (uint8_t)(analog[i] / 16U);
    }
    return true;
}

void LF_SensorUart_GetStats(LF_SensorUartStats *out_stats)
{
    uint32_t primask;

    if (out_stats == NULL) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    out_stats->rx_byte_count = s_stats.rx_byte_count;
    out_stats->analog_frame_count = s_stats.analog_frame_count;
    out_stats->digital_frame_count = s_stats.digital_frame_count;
    out_stats->parse_error_count = s_stats.parse_error_count;
    out_stats->uart_error_count = s_stats.uart_error_count;
    out_stats->last_frame_ms = s_stats.last_frame_ms;
    out_stats->last_rx_byte = s_stats.last_rx_byte;
    out_stats->analog_frame_valid = s_stats.analog_frame_valid;
    out_stats->digital_frame_valid = s_stats.digital_frame_valid;
    __set_PRIMASK(primask);
}

void LF_SensorUart_GetLastAsciiFrame(char out_frame[LF_SENSOR_UART_MAX_FRAME_LEN])
{
    uint32_t primask;
    uint32_t i;

    if (out_frame == NULL) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    for (i = 0U; i < LF_SENSOR_UART_MAX_FRAME_LEN; ++i) {
        out_frame[i] = (char)s_last_ascii_frame[i];
        if (out_frame[i] == '\0') {
            break;
        }
    }
    if (i >= LF_SENSOR_UART_MAX_FRAME_LEN) {
        out_frame[LF_SENSOR_UART_MAX_FRAME_LEN - 1U] = '\0';
    }
    __set_PRIMASK(primask);
}

uint32_t LF_SensorUart_GetValidFrameCount(void)
{
    return s_stats.analog_frame_count + s_stats.digital_frame_count;
}

uint32_t LF_SensorUart_GetErrorCount(void)
{
    return s_stats.parse_error_count + s_stats.uart_error_count;
}
