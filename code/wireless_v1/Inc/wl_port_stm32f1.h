/**
 * @file    wl_port_stm32f1.h
 * @brief   STM32F103C8T6 HAL 端口 — EWM22A 模块的引脚与外设映射。
 *
 * 根据实际接线修改此文件即可。无线模块其余代码仅引用
 * wl_platform.h，与具体板级无关。
 *
 * 编译开关：定义宏 WL_USE_STM32F1_HAL_PORT 以启用真实 HAL 实现
 * （wl_platform_stm32f1.c）。
 */

#ifndef WL_PORT_STM32F1_H
#define WL_PORT_STM32F1_H

#ifdef WL_USE_STM32F1_HAL_PORT

#include "stm32f1xx_hal.h"

/* ------------------------------------------------------------------ */
/*  UART — 连接 EWM22A 的 TXD / RXD                                   */
/* ------------------------------------------------------------------ */

/** 连接 EWM22A 模块的 USART 外设。 */
#define WL_UART_INSTANCE            USART2

/** HAL 句柄 — 声明为 extern，定义在 wl_platform_stm32f1.c 中。 */
extern UART_HandleTypeDef           wl_huart;

/* 发送引脚 */
#define WL_UART_TX_PORT             GPIOA
#define WL_UART_TX_PIN              GPIO_PIN_2      /* PA2（USART2_TX） */
#define WL_UART_TX_AF               GPIO_MODE_AF_PP

/* 接收引脚 */
#define WL_UART_RX_PORT             GPIOA
#define WL_UART_RX_PIN              GPIO_PIN_3      /* PA3（USART2_RX） */

/* ------------------------------------------------------------------ */
/*  AUX 引脚 — EWM22A 第 19 脚（输出，低电平 = 忙碌）                   */
/* ------------------------------------------------------------------ */
#define WL_AUX_PORT                 GPIOB
#define WL_AUX_PIN                  GPIO_PIN_0      /* PB0 */

/* ------------------------------------------------------------------ */
/*  可选：调试 UART（与模块 UART 分开）                                 */
/* ------------------------------------------------------------------ */
#define WL_DEBUG_UART_INSTANCE      USART1
extern UART_HandleTypeDef           wl_huart_debug;

#define WL_DEBUG_TX_PORT            GPIOA
#define WL_DEBUG_TX_PIN             GPIO_PIN_9      /* PA9（USART1_TX） */
#define WL_DEBUG_RX_PORT            GPIOA
#define WL_DEBUG_RX_PIN             GPIO_PIN_10     /* PA10（USART1_RX） */

/* ------------------------------------------------------------------ */
/*  UART 接收 DMA（可选 — 高波特率下可提高可靠性）                       */
/* ------------------------------------------------------------------ */
/* 如需启用 DMA 接收，取消以下注释：
 * #define WL_UART_USE_DMA
 * #define WL_UART_DMA_CHANNEL       DMA1_Channel6
 */

/* ------------------------------------------------------------------ */
/*  中断优先级                                                         */
/* ------------------------------------------------------------------ */
#define WL_UART_IRQ_PRIORITY        5
#define WL_UART_IRQ_SUB_PRIORITY    0

#endif /* WL_USE_STM32F1_HAL_PORT */
#endif /* WL_PORT_STM32F1_H */
