#ifndef WL_PORT_STM32F4_H
#define WL_PORT_STM32F4_H

#ifdef WL_USE_STM32F4_HAL_PORT

#include "stm32f4xx_hal.h"

/* ===== EWM22A LoRa UART（与已上板验证代码一致：USART1 PA9/PA10） ===== */
#define WL_UART_INSTANCE (USART1)
extern UART_HandleTypeDef wl_huart;

#define WL_UART_TX_PORT (GPIOA)
#define WL_UART_TX_PIN (GPIO_PIN_9)
#define WL_UART_TX_AF (GPIO_AF7_USART1)

#define WL_UART_RX_PORT (GPIOA)
#define WL_UART_RX_PIN (GPIO_PIN_10)
#define WL_UART_RX_AF (GPIO_AF7_USART1)

#define WL_UART_IRQN (USART1_IRQn)
#define WL_UART_IRQ_PRIORITY (5U)
#define WL_UART_IRQ_SUB_PRIORITY (0U)

/* ===== EWM22A AUX 引脚（默认 PB2，按实际接线修改） ===== */
#define WL_AUX_PORT (GPIOB)
#define WL_AUX_PIN (GPIO_PIN_2)

/* ===== EWM22A RST 引脚（与已上板验证代码一致：PA4，低电平复位） ===== */
#define WL_RST_PORT (GPIOA)
#define WL_RST_PIN (GPIO_PIN_4)
#define WL_RST_ACTIVE_LEVEL (GPIO_PIN_RESET)
#define WL_RST_LOW_TIME_MS (10U)
#define WL_RST_HIGH_TIME_MS (100U)

/* LoRa 已占用 USART1，默认不单独占用调试串口。 */
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
