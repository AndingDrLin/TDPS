#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void Error_Handler(void);

#define RADAR_GPIO_Pin GPIO_PIN_0
#define RADAR_GPIO_GPIO_Port GPIOB

#define DEBUG_TX_Pin GPIO_PIN_2
#define DEBUG_TX_GPIO_Port GPIOA
#define DEBUG_RX_Pin GPIO_PIN_3
#define DEBUG_RX_GPIO_Port GPIOA

#define RADAR_TX_Pin GPIO_PIN_10
#define RADAR_TX_GPIO_Port GPIOB
#define RADAR_RX_Pin GPIO_PIN_11
#define RADAR_RX_GPIO_Port GPIOB

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
