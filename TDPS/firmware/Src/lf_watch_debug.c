#include "lf_watch_debug.h"

#include <stddef.h>

#include "lf_debug_monitor.h"

#ifdef LF_USE_STM32F4_HAL_PORT
#include "lf_port_stm32f4_hal.h"
#endif

volatile LF_WatchDebug g_lf_watch;

void LF_WatchDebug_UpdateApp(const LF_AppContext *ctx)
{
    uint32_t i;

    if (ctx == NULL) {
        return;
    }

    g_lf_watch.app_state = (uint32_t)ctx->state;
    g_lf_watch.reason = (uint32_t)ctx->reason;
    g_lf_watch.line_detected = ctx->last_frame.line_detected ? 1U : 0U;
    g_lf_watch.position = ctx->last_frame.position;
    g_lf_watch.signal_sum = ctx->last_frame.signal_sum;
    g_lf_watch.peak_value = ctx->last_frame.peak_value;
    g_lf_watch.contrast_value = ctx->last_frame.contrast_value;
    g_lf_watch.no_car_mode = LF_DebugMonitor_IsNoCarMode() ? 1U : 0U;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        g_lf_watch.raw[i] = ctx->last_frame.raw[i];
        g_lf_watch.norm[i] = ctx->last_frame.norm[i];
    }

#ifdef LF_USE_STM32F4_HAL_PORT
    g_lf_watch.tim8_ccr3 = TIM8->CCR3;
    g_lf_watch.tim10_ccr1 = TIM10->CCR1;
    g_lf_watch.gpio_b_idr = GPIOB->IDR;
    g_lf_watch.gpio_b_odr = GPIOB->ODR;
    g_lf_watch.gpio_c_odr = GPIOC->ODR;
#endif
}

void LF_WatchDebug_UpdateMotor(int16_t left_cmd, int16_t right_cmd, uint32_t no_car_mode)
{
    g_lf_watch.left_cmd = left_cmd;
    g_lf_watch.right_cmd = right_cmd;
    g_lf_watch.no_car_mode = no_car_mode;

#ifdef LF_USE_STM32F4_HAL_PORT
    g_lf_watch.tim8_ccr3 = TIM8->CCR3;
    g_lf_watch.tim10_ccr1 = TIM10->CCR1;
    g_lf_watch.gpio_b_idr = GPIOB->IDR;
    g_lf_watch.gpio_b_odr = GPIOB->ODR;
    g_lf_watch.gpio_c_odr = GPIOC->ODR;
#endif
}
