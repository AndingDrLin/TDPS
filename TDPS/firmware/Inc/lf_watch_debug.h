/**
 * @file lf_watch_debug.h
 * @brief Keil Watch window debug snapshot structure.
 */
#ifndef LF_WATCH_DEBUG_H
#define LF_WATCH_DEBUG_H

#include <stdint.h>

#include "lf_app.h"

#ifdef LF_USE_STM32F4_HAL_PORT
#include "stm32f4xx_hal.h"
#endif

/** Debug snapshot structure for Keil Watch window inspection. */
typedef struct {
    volatile uint32_t tick_ms;             /**< Current system tick in milliseconds */
    volatile uint32_t app_state;           /**< Current application state */
    volatile uint32_t reason;              /**< State transition reason code */
    volatile uint32_t line_detected;       /**< Whether line is currently detected */
    volatile int32_t position;             /**< Weighted sensor position */
    volatile uint32_t signal_sum;          /**< Sum of all channel values */
    volatile uint32_t peak_value;          /**< Peak channel value */
    volatile uint32_t contrast_value;      /**< Contrast value (peak - background) */
    volatile uint16_t raw[LF_SENSOR_COUNT]; /**< Raw sensor values */
    volatile uint16_t norm[LF_SENSOR_COUNT]; /**< Normalized sensor values */
    /* Individual raw/norm values for quick inspection */
    volatile uint16_t raw0;
    volatile uint16_t raw1;
    volatile uint16_t raw2;
    volatile uint16_t raw3;
    volatile uint16_t raw4;
    volatile uint16_t raw5;
    volatile uint16_t raw6;
    volatile uint16_t raw7;
    volatile uint16_t norm0;
    volatile uint16_t norm1;
    volatile uint16_t norm2;
    volatile uint16_t norm3;
    volatile uint16_t norm4;
    volatile uint16_t norm5;
    volatile uint16_t norm6;
    volatile uint16_t norm7;
    volatile int16_t left_cmd;             /**< Left motor command */
    volatile int16_t right_cmd;            /**< Right motor command */
    volatile uint32_t no_car_mode;         /**< No-car (bench debug) mode flag */
    volatile uint32_t boot_elapsed_ms;     /**< Time elapsed since boot (ms) */
    volatile uint32_t wait_line_since_ms;  /**< Timestamp when line detection wait started */
    volatile uint32_t start_button_pressed; /**< Start button pressed flag */
    volatile uint32_t auto_start_delay_ms; /**< Auto-start delay configuration */
    volatile uint32_t start_min_boot_delay_ms; /**< Minimum boot delay configuration */
    volatile uint32_t start_line_hold_ms;  /**< Line hold duration configuration */
    volatile uint32_t tim8_ccr3;           /**< TIM8 CCR3 register value */
    volatile uint32_t tim10_ccr1;          /**< TIM10 CCR1 register value */
    volatile uint32_t gpio_b_idr;          /**< GPIOB input data register */
    volatile uint32_t gpio_b_odr;          /**< GPIOB output data register */
    volatile uint32_t gpio_c_odr;          /**< GPIOC output data register */
} LF_WatchDebug;

extern volatile LF_WatchDebug g_lf_watch;

/**
 * @brief Update the watch debug snapshot from the application context.
 * @param ctx Pointer to the current application context.
 */
void LF_WatchDebug_UpdateApp(const LF_AppContext *ctx);

/**
 * @brief Update the watch debug snapshot with motor command data.
 * @param left_cmd    Left motor command.
 * @param right_cmd   Right motor command.
 * @param no_car_mode Non-zero if in no-car (bench) mode.
 */
void LF_WatchDebug_UpdateMotor(int16_t left_cmd, int16_t right_cmd, uint32_t no_car_mode);

#endif /* LF_WATCH_DEBUG_H */
