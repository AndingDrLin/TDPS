#ifndef LF_WATCH_DEBUG_H
#define LF_WATCH_DEBUG_H

#include <stdint.h>

#include "lf_app.h"

#ifdef LF_USE_STM32F4_HAL_PORT
#include "stm32f4xx_hal.h"
#endif

typedef struct {
    volatile uint32_t tick_ms;
    volatile uint32_t app_state;
    volatile uint32_t reason;
    volatile uint32_t line_detected;
    volatile int32_t position;
    volatile uint32_t signal_sum;
    volatile uint32_t peak_value;
    volatile uint32_t contrast_value;
    volatile uint16_t raw[LF_SENSOR_COUNT];
    volatile uint16_t norm[LF_SENSOR_COUNT];
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
    volatile int16_t left_cmd;
    volatile int16_t right_cmd;
    volatile uint32_t no_car_mode;
    volatile uint32_t boot_elapsed_ms;
    volatile uint32_t wait_line_since_ms;
    volatile uint32_t start_button_pressed;
    volatile uint32_t auto_start_delay_ms;
    volatile uint32_t start_min_boot_delay_ms;
    volatile uint32_t start_line_hold_ms;
    volatile uint32_t tim8_ccr3;
    volatile uint32_t tim10_ccr1;
    volatile uint32_t gpio_b_idr;
    volatile uint32_t gpio_b_odr;
    volatile uint32_t gpio_c_odr;
} LF_WatchDebug;

extern volatile LF_WatchDebug g_lf_watch;

void LF_WatchDebug_UpdateApp(const LF_AppContext *ctx);
void LF_WatchDebug_UpdateMotor(int16_t left_cmd, int16_t right_cmd, uint32_t no_car_mode);

#endif /* LF_WATCH_DEBUG_H */
