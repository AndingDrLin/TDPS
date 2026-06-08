/**
 * @file lf_ultrasonic_platform.h
 * @brief Platform abstraction for the ultrasonic sensor.
 *
 * Each platform implementation (STM32 HAL, PC stub) provides these
 * functions for hardware-specific GPIO and timing operations.
 */
#ifndef LF_ULTRASONIC_PLATFORM_H
#define LF_ULTRASONIC_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the ultrasonic sensor GPIO pins.
 *
 * Configures the TRIG pin as push-pull output (idle low) and the
 * ECHO pin as input (with pull-down if available).
 */
void LF_Ultrasonic_Platform_Init(void);

/**
 * @brief Send a 10us trigger pulse on the TRIG pin.
 */
void LF_Ultrasonic_Platform_Trigger(void);

/**
 * @brief Read the current state of the ECHO pin.
 * @return true if ECHO is high, false if low.
 */
bool LF_Ultrasonic_Platform_ReadEcho(void);

/**
 * @brief Busy-wait for a specified number of microseconds.
 * @param us Microseconds to wait.
 */
void LF_Ultrasonic_Platform_DelayUs(uint32_t us);

/**
 * @brief Get the current time in microseconds.
 * @return Microsecond timestamp.
 */
uint32_t LF_Ultrasonic_Platform_GetMicros(void);

#endif /* LF_ULTRASONIC_PLATFORM_H */
