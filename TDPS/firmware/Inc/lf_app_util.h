/**
 * @file lf_app_util.h
 * @brief Internal utility functions for the line-following application.
 */
#ifndef LF_APP_UTIL_H
#define LF_APP_UTIL_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_app.h"

/**
 * @brief Absolute value of a 32-bit signed integer.
 * @param value Input value.
 * @return Non-negative absolute value.
 */
int32_t lf_abs_i32(int32_t value);

/**
 * @brief Check whether a time duration has elapsed.
 *
 * Uses unsigned wrapping arithmetic so it tolerates timer overflow.
 *
 * @param now   Current timestamp.
 * @param start Timestamp when the interval began.
 * @param duration Required elapsed ticks.
 * @return true if (now - start) >= duration.
 */
bool lf_time_reached(uint32_t now, uint32_t start, uint32_t duration);

/**
 * @brief Increment a uint8_t counter, clamping at UINT8_MAX.
 * @param value Pointer to the counter to increment.
 */
void lf_bump_u8(uint8_t *value);

/**
 * @brief Clamp motor speed when sensor calibration is degraded.
 *
 * If calibration degraded, speed is limited to
 * g_lf_config.sensor_degraded_max_speed.
 *
 * @param speed Requested speed.
 * @return Clamped speed.
 */
int16_t lf_limit_degraded_speed(int16_t speed);

/**
 * @brief Transition the application state machine and update LED pattern.
 * @param state Target state.
 */
void lf_set_state(LF_AppState state);

#endif /* LF_APP_UTIL_H */
