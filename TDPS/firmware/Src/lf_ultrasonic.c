/**
 * @file lf_ultrasonic.c
 * @brief Ultrasonic distance sensor driver for overhead obstacle detection.
 *
 * Implements a non-blocking measurement cycle:
 * 1. Send 10us trigger pulse
 * 2. Wait for ECHO to go high (with timeout)
 * 3. Measure ECHO pulse width
 * 4. Convert to distance: distance_cm = pulse_us / 58
 *
 * The measurement is triggered periodically by LF_Ultrasonic_Tick().
 * A simple state machine avoids blocking the main loop.
 */
#include "lf_ultrasonic.h"

#include "lf_config.h"
#include "lf_ultrasonic_platform.h"

/* ==================== Measurement State Machine ==================== */

typedef enum {
    US_STATE_IDLE = 0,         /**< Waiting for next measurement window */
    US_STATE_TRIGGERED,        /**< Trigger pulse sent, waiting for echo rising edge */
    US_STATE_ECHO_HIGH,        /**< Echo is high, measuring pulse width */
} UltrasonicMeasureState;

/** Time of the last measurement start (for measurement interval). */
static uint32_t s_last_measure_ms = 0U;

/** Measurement state machine state. */
static UltrasonicMeasureState s_measure_state = US_STATE_IDLE;

/** Timestamp when the trigger pulse was sent. */
static uint32_t s_trigger_us = 0U;

/** Timestamp when ECHO went high. */
static uint32_t s_echo_start_us = 0U;

/** Latest reading result. */
static LF_UltrasonicReading s_reading = {
    .state = LF_ULTRASONIC_CLEAR,
    .distance_mm = 0U,
    .echo_valid = false,
    .last_update_ms = 0U,
};

/* ==================== Distance Threshold Check ==================== */

/**
 * @brief Update the obstacle state based on the latest distance.
 *
 * If the measured distance is within the configured overhead threshold,
 * the state transitions to LF_ULTRASONIC_BLOCK. Otherwise it is CLEAR.
 */
static void update_obstacle_state(void)
{
    uint16_t threshold = g_lf_config.ultrasonic_threshold_mm;

    if (!s_reading.echo_valid || threshold == 0U) {
        s_reading.state = LF_ULTRASONIC_CLEAR;
        return;
    }

    /* Obstacle detected when distance is less than threshold. */
    if (s_reading.distance_mm > 0U && s_reading.distance_mm < threshold) {
        s_reading.state = LF_ULTRASONIC_BLOCK;
    } else {
        s_reading.state = LF_ULTRASONIC_CLEAR;
    }
}

/* ==================== Public Interface ==================== */

void LF_Ultrasonic_Init(void)
{
    LF_Ultrasonic_Platform_Init();

    s_measure_state = US_STATE_IDLE;
    s_last_measure_ms = 0U;
    s_trigger_us = 0U;
    s_echo_start_us = 0U;
    s_reading.state = LF_ULTRASONIC_CLEAR;
    s_reading.distance_mm = 0U;
    s_reading.echo_valid = false;
    s_reading.last_update_ms = 0U;
}

void LF_Ultrasonic_Tick(uint32_t now_ms)
{
    uint32_t now_us;
    uint32_t elapsed_us;
    uint32_t pulse_us;

    /* Skip if ultrasonic is disabled. */
    if (!g_lf_config.ultrasonic_enable) {
        s_reading.state = LF_ULTRASONIC_CLEAR;
        return;
    }

    now_us = LF_Ultrasonic_Platform_GetMicros();

    switch (s_measure_state) {
    case US_STATE_IDLE:
        /* Wait for the measurement interval before starting a new reading. */
        if ((uint32_t)(now_ms - s_last_measure_ms) < g_lf_config.ultrasonic_measure_interval_ms) {
            return;
        }

        /* Send the 10us trigger pulse. */
        LF_Ultrasonic_Platform_Trigger();
        s_trigger_us = now_us;
        s_measure_state = US_STATE_TRIGGERED;
        break;

    case US_STATE_TRIGGERED:
        /* Wait for ECHO to go high (timeout after ~25ms = ~4m range). */
        elapsed_us = (uint32_t)(now_us - s_trigger_us);
        if (elapsed_us > 25000U) {
            /* Timeout: no echo received. */
            s_reading.echo_valid = false;
            s_reading.distance_mm = 0U;
            s_reading.last_update_ms = now_ms;
            s_last_measure_ms = now_ms;
            update_obstacle_state();
            s_measure_state = US_STATE_IDLE;
            return;
        }

        if (LF_Ultrasonic_Platform_ReadEcho()) {
            s_echo_start_us = now_us;
            s_measure_state = US_STATE_ECHO_HIGH;
        }
        break;

    case US_STATE_ECHO_HIGH:
        /* Wait for ECHO to go low (timeout after ~25ms). */
        elapsed_us = (uint32_t)(now_us - s_echo_start_us);
        if (elapsed_us > 25000U) {
            /* Timeout while ECHO is high: invalid reading. */
            s_reading.echo_valid = false;
            s_reading.distance_mm = 0U;
            s_reading.last_update_ms = now_ms;
            s_last_measure_ms = now_ms;
            update_obstacle_state();
            s_measure_state = US_STATE_IDLE;
            return;
        }

        if (!LF_Ultrasonic_Platform_ReadEcho()) {
        /* Calculate distance: distance_cm = pulse_us / 58.
         * Use safe computation to avoid overflow for large pulse values. */
            pulse_us = (uint32_t)(now_us - s_echo_start_us);
            s_reading.distance_mm = (uint16_t)((pulse_us / 58U) * 10U);
            s_reading.echo_valid = true;
            s_reading.last_update_ms = now_ms;
            s_last_measure_ms = now_ms;
            update_obstacle_state();
            s_measure_state = US_STATE_IDLE;
        }
        break;
    }
}

const LF_UltrasonicReading *LF_Ultrasonic_GetReading(void)
{
    return &s_reading;
}
