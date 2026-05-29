#ifndef WL_PORT_STM32F4_H
#define WL_PORT_STM32F4_H

#ifdef WL_USE_STM32F4_HAL_PORT

#include "stm32f4xx_hal.h"

/* ===== EWM22A LoRa UART（当前 TDPS PCB：UART5 PC12/PD2） ===== */
#define WL_UART_INSTANCE (UART5)
extern UART_HandleTypeDef wl_huart;

#define WL_UART_TX_PORT (GPIOC)
#define WL_UART_TX_PIN (GPIO_PIN_12)
#define WL_UART_TX_AF (GPIO_AF8_UART5)

#define WL_UART_RX_PORT (GPIOD)
#define WL_UART_RX_PIN (GPIO_PIN_2)
#define WL_UART_RX_AF (GPIO_AF8_UART5)

#define WL_UART_IRQN (UART5_IRQn)
#define WL_UART_IRQ_PRIORITY (5U)
#define WL_UART_IRQ_SUB_PRIORITY (0U)

/* ===== EWM22A GPIO（当前 TDPS PCB） ===== */
#define WL_AUX_PORT (GPIOE)
#define WL_AUX_PIN (GPIO_PIN_0)

#define WL_LINK_PORT (GPIOD)
#define WL_LINK_PIN (GPIO_PIN_0)

#define WL_WAKE_PORT (GPIOD)
#define WL_WAKE_PIN (GPIO_PIN_1)
#define WL_WAKE_ACTIVE_LEVEL (GPIO_PIN_RESET)

#define WL_RST_PORT (GPIOE)
#define WL_RST_PIN (GPIO_PIN_1)
#define WL_RST_ACTIVE_LEVEL (GPIO_PIN_RESET)
#define WL_RST_LOW_TIME_MS (50U)
#define WL_RST_HIGH_TIME_MS (100U)

/* LoRa 使用 UART5，默认不单独占用调试串口。 */
#define WL_ENABLE_DEBUG_UART (0)
#define WL_DEBUG_UART_INSTANCE (USART3)
extern UART_HandleTypeDef wl_huart_debug;

#define WL_DEBUG_TX_PORT (GPIOB)
#define WL_DEBUG_TX_PIN (GPIO_PIN_10)
#define WL_DEBUG_TX_AF (GPIO_AF7_USART3)

#define WL_DEBUG_RX_PORT (GPIOB)
#define WL_DEBUG_RX_PIN (GPIO_PIN_11)
#define WL_DEBUG_RX_AF (GPIO_AF7_USART3)

void WL_Port_UartRxCpltCallback(UART_HandleTypeDef *huart);
void WL_Port_UartErrorCallback(UART_HandleTypeDef *huart);
void WL_Port_UartTxCpltCallback(UART_HandleTypeDef *huart);

#endif /* WL_USE_STM32F4_HAL_PORT */

#endif /* WL_PORT_STM32F4_H */
