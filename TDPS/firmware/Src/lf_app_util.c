/**
 * @file lf_app_util.c
 * @brief Internal utility functions for the line-following application.
 *
 * Provides timing helpers, counter management, speed limiting, and
 * state-transition logic shared across all lf_app sub-modules.
 */
#include "lf_app_util.h"

#include <limits.h>

#include "lf_app_internal.h"
#include "lf_config.h"
#include "lf_led_blink.h"

/* ==================== Utility Functions ==================== */

int32_t lf_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

bool lf_time_reached(uint32_t now, uint32_t start, uint32_t duration)
{
    /* Wrapping subtraction handles timer overflow correctly. */
    return (uint32_t)(now - start) >= duration;
}

void lf_bump_u8(uint8_t *value)
{
    if (value != 0 && *value < UINT8_MAX) {
        *value += 1U;
    }
}

int16_t lf_limit_degraded_speed(int16_t speed)
{
    int16_t limit;

    if (!g_app.calibration_degraded) {
        return speed;
    }

    /* When sensor calibration is degraded, cap motor speed to avoid
     * aggressive maneuvers on unreliable sensor data. */
    limit = g_lf_config.sensor_degraded_max_speed;
    if (limit <= 0) {
        return speed;
    }
    if (speed > limit) {
        return limit;
    }
    if (speed < (int16_t)(-limit)) {
        return (int16_t)(-limit);
    }
    return speed;
}

/* ==================== State Transition ==================== */

void lf_set_state(LF_AppState state)
{
    if (g_app.state == state) {
        return;
    }

    g_app.state = state;

    /* Update the LED blink pattern to reflect the new state. */
    switch (state) {
    case LF_APP_STATE_WAIT_START:
        LF_LedBlink_SetPattern(LF_LED_SLOW_BLINK);
        break;
    case LF_APP_STATE_CALIBRATING:
        LF_LedBlink_SetPattern(LF_LED_FAST_BLINK);
        break;
    case LF_APP_STATE_RUNNING:
        LF_LedBlink_SetPattern(LF_LED_ON);
        break;
    case LF_APP_STATE_RECOVERING:
        LF_LedBlink_SetPattern(LF_LED_TRIPLE_FLASH);
        break;
    case LF_APP_STATE_AVOID_PREP:
    case LF_APP_STATE_AVOID_BYPASS:
    case LF_APP_STATE_AVOID_REACQUIRE:
    case LF_APP_STATE_FIXED_TURN_STOP:
    case LF_APP_STATE_FIXED_TURN_SPIN:
    case LF_APP_STATE_FIXED_TURN_SETTLE:
    case LF_APP_STATE_REORIENT_STOP:
    case LF_APP_STATE_REORIENT_SPIN:
    case LF_APP_STATE_REORIENT_CONFIRM:
    case LF_APP_STATE_ULTRASONIC_HOLD:
        LF_LedBlink_SetPattern(LF_LED_HEARTBEAT);
        break;
    case LF_APP_STATE_STOPPED:
    case LF_APP_STATE_FAULT:
    default:
        break;
    }
}
