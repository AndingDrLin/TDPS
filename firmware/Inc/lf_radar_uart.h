#ifndef LF_RADAR_UART_H
#define LF_RADAR_UART_H

#include <stdint.h>

#ifdef LF_USE_STM32F4_HAL_PORT
#include "stm32f4xx_hal.h"
#endif

void LF_RadarUart_Init(void);
uint16_t LF_RadarUart_Read(uint8_t *out_buf, uint16_t max_len);

#ifdef LF_USE_STM32F4_HAL_PORT
void LF_RadarUart_RxCpltCallback(UART_HandleTypeDef *huart);
void LF_RadarUart_ErrorCallback(UART_HandleTypeDef *huart);
#endif

#endif /* LF_RADAR_UART_H */
