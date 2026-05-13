#ifndef LF_PORT_STM32F4_HAL_H
#define LF_PORT_STM32F4_HAL_H

/*
 * STM32F4 HAL 端口配置
 * 使用方法：
 * 1. 在工程编译宏里添加 LF_USE_STM32F4_HAL_PORT。
 * 2. 按你们板级连接修改下列外设句柄与引脚宏。
 * 3. 保证 ADC 已配置为 DMA 连续采样，缓冲区长度 >= LF_SENSOR_COUNT。
 */

#include "stm32f4xx_hal.h"

#include "lf_config.h"

/* ===== 外设句柄（默认示例，按实际工程修改） ===== */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* ===== 传感器 DMA 缓冲（在用户文件中定义） ===== */
extern volatile uint32_t g_lf_sensor_dma_buffer[LF_SENSOR_COUNT];

/* ===== ADC ===== */
#define LF_PORT_ADC_HANDLE (hadc1)

/* ===== 左右电机 PWM ===== */
#define LF_PORT_LEFT_PWM_TIMER_HANDLE (htim3)
#define LF_PORT_LEFT_PWM_CHANNEL (TIM_CHANNEL_1)
#define LF_PORT_RIGHT_PWM_TIMER_HANDLE (htim3)
#define LF_PORT_RIGHT_PWM_CHANNEL (TIM_CHANNEL_2)

/* ===== 左右电机方向脚 ===== */
#define LF_PORT_LEFT_DIR_GPIO_PORT (GPIOB)
#define LF_PORT_LEFT_DIR_PIN (GPIO_PIN_0)
#define LF_PORT_RIGHT_DIR_GPIO_PORT (GPIOB)
#define LF_PORT_RIGHT_DIR_PIN (GPIO_PIN_1)

/* 前进方向对应电平。若电机方向反了，只需改这里。 */
#define LF_PORT_LEFT_FORWARD_LEVEL (GPIO_PIN_SET)
#define LF_PORT_RIGHT_FORWARD_LEVEL (GPIO_PIN_SET)

/* ===== 状态灯 ===== */
#define LF_PORT_LED_GPIO_PORT (GPIOC)
#define LF_PORT_LED_PIN (GPIO_PIN_13)
#define LF_PORT_LED_ON_LEVEL (GPIO_PIN_RESET)

/* ===== 启动按钮 ===== */
#define LF_PORT_BTN_GPIO_PORT (GPIOA)
#define LF_PORT_BTN_PIN (GPIO_PIN_0)
#define LF_PORT_BTN_PRESSED_LEVEL (GPIO_PIN_RESET)

/* ===== 调试串口（可选） ===== */
#define LF_PORT_ENABLE_DEBUG_UART (1)
#define LF_PORT_DEBUG_UART_HANDLE (huart1)

/* ===== 雷达串口（默认 USART2，按实际接线修改） ===== */
#define LF_PORT_RADAR_UART_HANDLE (huart2)
#define LF_PORT_RADAR_UART_IRQN (USART2_IRQn)
#define LF_PORT_RADAR_UART_IRQ_PRIORITY (5U)
#define LF_PORT_RADAR_UART_IRQ_SUB_PRIORITY (0U)
#define LF_PORT_RADAR_RX_BUFFER_SIZE (512U)

void LF_Port_UartRxCpltCallback(UART_HandleTypeDef *huart);
void LF_Port_UartErrorCallback(UART_HandleTypeDef *huart);
void LF_Port_UartTxCpltCallback(UART_HandleTypeDef *huart);

#endif /* LF_PORT_STM32F4_HAL_H */
