/**
 * @file lf_led_blink.h
 * @brief Status LED blinking pattern engine.
 */
#ifndef LF_LED_BLINK_H
#define LF_LED_BLINK_H

#include <stdbool.h>
#include <stdint.h>

/** LED blink pattern identifiers. */
typedef enum {
    LF_LED_OFF = 0,            /**< LED permanently off */
    LF_LED_ON,                 /**< LED permanently on */
    LF_LED_SLOW_BLINK,         /**< 1 Hz: 500ms on, 500ms off */
    LF_LED_FAST_BLINK,         /**< 4 Hz: 125ms on, 125ms off */
    LF_LED_DOUBLE_FLASH,       /**< 2 quick flashes + long pause */
    LF_LED_TRIPLE_FLASH,       /**< 3 quick flashes + long pause */
    LF_LED_HEARTBEAT,          /**< Asymmetric short-long-short */
    LF_LED_RAPID_PULSE,        /**< 8 Hz fast blink */
    LF_LED_POSTMORTEM,         /**< Blinks numeric code then stats */
} LF_LedPattern;

/** @brief Initialize the LED blink engine to OFF state. */
void LF_LedBlink_Init(void);

/**
 * @brief Request a new LED blink pattern.
 *
 * Post-mortem mode takes priority and cannot be overridden.
 *
 * @param pattern Desired LED pattern.
 */
void LF_LedBlink_SetPattern(LF_LedPattern pattern);

/**
 * @brief Enter post-mortem mode and set numeric blink codes.
 *
 * Cycles: blink reason_code times (250ms on/off), pause 2s,
 * blink stats_code times (250ms on/off), pause 3s, repeat.
 *
 * @param reason_code  Blink count for the reason phase (1-255).
 * @param stats_code   Blink count for the stats phase (0-15, 0 = skip).
 */
void LF_LedBlink_SetPostmortemCode(uint8_t reason_code, uint8_t stats_code);

/** @brief Non-blocking LED tick. Call every main-loop iteration. */
void LF_LedBlink_Tick(void);

#endif /* LF_LED_BLINK_H */
