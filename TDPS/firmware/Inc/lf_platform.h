/**
 * @file lf_platform.h
 * @brief Platform Abstraction Layer (PAL) for the line-following module.
 *
 * Goals:
 * 1. Keep business code independent of specific MCU peripheral details.
 * 2. When switching to a different hardware platform, only this layer's
 *    implementation needs to be replaced.
 */
#ifndef LF_PLATFORM_H
#define LF_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_config.h"

/** @brief Board initialization: clocks, GPIO, PWM, ADC, DMA, UART, etc. */
void LF_Platform_BoardInit(void);

/** @brief Read the monotonically increasing millisecond counter. */
uint32_t LF_Platform_GetMillis(void);

/** @brief Read the monotonically increasing microsecond counter (used for high-precision dt_s calculation). */
uint32_t LF_Platform_GetMicros(void);

/**
 * @brief Blocking millisecond delay.
 *
 * Intended for initialization phase only; not recommended in the control loop.
 *
 * @param ms Delay duration in milliseconds.
 */
void LF_Platform_DelayMs(uint32_t ms);

/**
 * @brief Read raw line-sensor values.
 *
 * Range is determined by ADC resolution (e.g. 0-4095).
 *
 * @param[out] out_raw Array of LF_SENSOR_COUNT elements to receive the raw values.
 */
void LF_Platform_ReadLineSensorRaw(uint16_t out_raw[LF_SENSOR_COUNT]);

/**
 * @brief Non-blocking read from the radar UART buffer.
 *
 * @param[out] out_buf Destination buffer.
 * @param      max_len Maximum number of bytes to read.
 * @return     Actual number of bytes read; 0 if no data available.
 */
uint16_t LF_Platform_RadarRead(uint8_t *out_buf, uint16_t max_len);

#ifndef LF_USE_STM32F4_HAL_PORT
void LF_PlatformStub_RadarInject(const uint8_t *data, uint16_t len);
void LF_PlatformStub_RadarClear(void);
void LF_PlatformStub_SetLineSensorRaw(const uint16_t raw[LF_SENSOR_COUNT]);
void LF_PlatformStub_ClearLineSensorRaw(void);
void LF_PlatformStub_SetDigitalFrame(const uint8_t digital[LF_SENSOR_COUNT]);
const char *LF_PlatformStub_GetLastDebugLine(void);
#endif

/**
 * @brief Write motor commands (-1000~1000).
 *
 * Sign indicates direction; absolute value indicates speed.
 *
 * @param left_cmd  Left wheel command.
 * @param right_cmd Right wheel command.
 */
void LF_Platform_SetMotorCommand(int16_t left_cmd, int16_t right_cmd);

/** @brief Status LED control.
 *  @param on true to turn on, false to turn off.
 */
void LF_Platform_SetStatusLed(bool on);

/** @brief Read the start button state.
 *  @return true if the button is pressed.
 */
bool LF_Platform_IsStartButtonPressed(void);

/**
 * @brief Lightweight debug print.
 *
 * May be a no-op if no UART is available.
 *
 * @param msg Null-terminated message string.
 */
void LF_Platform_DebugPrint(const char *msg);

#endif /* LF_PLATFORM_H */
