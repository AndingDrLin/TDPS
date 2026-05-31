#ifndef LF_SENSOR_UART_H
#define LF_SENSOR_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_config.h"

#ifdef LF_USE_STM32F4_HAL_PORT
#include "stm32f4xx_hal.h"
#endif

#define LF_SENSOR_UART_BAUDRATE          115200U
#define LF_SENSOR_UART_COMMAND_DIGITAL   "$0,0,1#"
#define LF_SENSOR_UART_COMMAND_ANALOG    "$0,1,0#"
#define LF_SENSOR_UART_COMMAND_BOTH      "$0,1,1#"
#define LF_SENSOR_UART_MAX_FRAME_LEN     128U

typedef struct {
    uint32_t rx_byte_count;
    uint32_t analog_frame_count;
    uint32_t digital_frame_count;
    uint32_t parse_error_count;
    uint32_t uart_error_count;
    uint32_t last_frame_ms;
    uint8_t last_rx_byte;
    uint8_t analog_frame_valid;
    uint8_t digital_frame_valid;
} LF_SensorUartStats;

void LF_SensorUart_Init(UART_HandleTypeDef *huart);
void LF_SensorUart_OnByte(UART_HandleTypeDef *huart, uint8_t byte);
void LF_SensorUart_RecordUartError(void);
HAL_StatusTypeDef LF_SensorUart_SendCommand(const char *cmd);
void LF_SensorUart_OnTxComplete(void);

bool LF_SensorUart_GetAnalogFrame(uint16_t out_values[LF_SENSOR_COUNT]);
bool LF_SensorUart_GetDigitalFrame(uint8_t out_values[LF_SENSOR_COUNT]);
bool LF_SensorUart_GetFrame(uint8_t out_values[LF_SENSOR_COUNT]);

void LF_SensorUart_GetStats(LF_SensorUartStats *out_stats);
void LF_SensorUart_GetLastAsciiFrame(char out_frame[LF_SENSOR_UART_MAX_FRAME_LEN]);
uint32_t LF_SensorUart_GetValidFrameCount(void);
uint32_t LF_SensorUart_GetErrorCount(void);

#endif /* LF_SENSOR_UART_H */
