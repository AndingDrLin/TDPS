#ifndef LF_SENSOR_UART_H
#define LF_SENSOR_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_config.h"

#ifdef LF_USE_STM32F4_HAL_PORT
#include "stm32f4xx_hal.h"
#endif

/*
 * UART 巡线传感器驱动
 *
 * 协议（模块自动发送，无需请求）：
 *   帧长 11 字节，波特率 115200 8N1，刷新率 ~10ms
 *   Byte 0:    0xAA（帧头）
 *   Byte 1~8:  S1~S8（8 路传感器原始值，0~255，越大越黑）
 *              S1 = 最左，S8 = 最右
 *   Byte 9:    (0xAA + S1 + ... + S8) mod 256（校验和）
 *   Byte 10:   固定 0x55（帧尾，可选校验）
 *
 * 前置 2 路辅助传感器：
 *   由另外 2 个独立传感器模块提供，通过 GPIO 读取。
 *   高电平 = 检测到黑线。
 */

#define LF_SENSOR_UART_FRAME_LEN     11U
#define LF_SENSOR_UART_HEADER        0xAAU
#define LF_SENSOR_UART_FOOTER        0x55U
#define LF_SENSOR_UART_BAUDRATE      115200U

/* 前置辅助传感器默认 GPIO 配置（可在 lf_port 中覆盖） */
#ifndef LF_PORT_FRONT_AUX_LEFT_PORT
#define LF_PORT_FRONT_AUX_LEFT_PORT  GPIOA
#define LF_PORT_FRONT_AUX_LEFT_PIN   GPIO_PIN_4
#endif
#ifndef LF_PORT_FRONT_AUX_RIGHT_PORT
#define LF_PORT_FRONT_AUX_RIGHT_PORT GPIOA
#define LF_PORT_FRONT_AUX_RIGHT_PIN  GPIO_PIN_5
#endif

void LF_SensorUart_Init(UART_HandleTypeDef *huart);

/* 在 HAL_UART_RxCpltCallback 中调用，由 platform 层传入接收到的字节。 */
void LF_SensorUart_OnByte(UART_HandleTypeDef *huart, uint8_t byte);

/* 获取最新有效帧的 8 路传感器值。返回 true 表示数据有效。 */
bool LF_SensorUart_GetFrame(uint8_t out_values[LF_SENSOR_COUNT]);

/* 获取前置 2 路辅助传感器状态（true = 检测到黑线）。 */
void LF_SensorUart_GetFrontAux(bool *out_left, bool *out_right);

/* 获取帧统计信息（调试用）。 */
uint32_t LF_SensorUart_GetValidFrameCount(void);
uint32_t LF_SensorUart_GetErrorCount(void);

#endif /* LF_SENSOR_UART_H */
