/**
 * @file lf_debug_monitor.c
 * @brief Lightweight debug monitor output.
 */
#include "lf_debug_monitor.h"

#include <stdint.h>
#include <stdio.h>

#include "lf_app.h"
#include "lf_config.h"
#include "lf_platform.h"
#include "lf_sensor.h"
#include "lf_watch_debug.h"
#include "wireless_hooks.h"
#include "wl_app.h"
#include "wl_lora.h"

#ifdef LF_USE_STM32F4_HAL_PORT
#include "lf_port_stm32f4_hal.h"
#endif

#ifndef TDPS_NO_CAR_MODE
#define TDPS_NO_CAR_MODE 0
#endif

#ifndef TDPS_DEBUG_MONITOR_ENABLE
#define TDPS_DEBUG_MONITOR_ENABLE 0
#endif

LF_DebugMonitorConfig g_lf_debug_monitor_config = {
    .enabled = (TDPS_DEBUG_MONITOR_ENABLE != 0),
    .no_car_mode = (TDPS_NO_CAR_MODE != 0),
    .report_period_ms = 200U,
    .report_line_raw = true,
    .report_wireless = true,
    .report_radar = true,
};

static uint32_t s_last_report_ms;
static int16_t s_last_left_cmd;
static int16_t s_last_right_cmd;

volatile LF_WatchDebug g_lf_watch;

/**
 * @brief Update the Keil Watch debug snapshot from the current application context.
 * @param ctx Pointer to the current application context (NULL-safe).
 */
void LF_WatchDebug_UpdateApp(const LF_AppContext *ctx)
{
    uint32_t i;

    if (ctx == NULL) {
        return;
    }

    g_lf_watch.app_state = (uint32_t)ctx->state;
    g_lf_watch.reason = 0U;
    g_lf_watch.line_detected = ctx->last_frame.line_detected ? 1U : 0U;
    g_lf_watch.position = ctx->last_frame.position;
    g_lf_watch.signal_sum = ctx->last_frame.signal_sum;
    g_lf_watch.peak_value = ctx->last_frame.peak_value;
    g_lf_watch.contrast_value = ctx->last_frame.contrast_value;
    g_lf_watch.no_car_mode = LF_DebugMonitor_IsNoCarMode() ? 1U : 0U;
    g_lf_watch.boot_elapsed_ms = LF_Platform_GetMillis() - ctx->boot_ms;
    g_lf_watch.wait_line_since_ms = ctx->wait_start_line_since_ms;
    g_lf_watch.start_button_pressed = LF_Platform_IsStartButtonPressed() ? 1U : 0U;
    g_lf_watch.auto_start_delay_ms = g_lf_config.auto_start_delay_ms;
    g_lf_watch.start_min_boot_delay_ms = g_lf_config.start_min_boot_delay_ms;
    g_lf_watch.start_line_hold_ms = g_lf_config.start_line_hold_ms;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        g_lf_watch.raw[i] = ctx->last_frame.raw[i];
        g_lf_watch.norm[i] = ctx->last_frame.norm[i];
    }
    g_lf_watch.raw0 = ctx->last_frame.raw[0];
    g_lf_watch.raw1 = ctx->last_frame.raw[1];
    g_lf_watch.raw2 = ctx->last_frame.raw[2];
    g_lf_watch.raw3 = ctx->last_frame.raw[3];
    g_lf_watch.raw4 = ctx->last_frame.raw[4];
    g_lf_watch.raw5 = ctx->last_frame.raw[5];
    g_lf_watch.raw6 = ctx->last_frame.raw[6];
    g_lf_watch.raw7 = ctx->last_frame.raw[7];
    g_lf_watch.norm0 = ctx->last_frame.norm[0];
    g_lf_watch.norm1 = ctx->last_frame.norm[1];
    g_lf_watch.norm2 = ctx->last_frame.norm[2];
    g_lf_watch.norm3 = ctx->last_frame.norm[3];
    g_lf_watch.norm4 = ctx->last_frame.norm[4];
    g_lf_watch.norm5 = ctx->last_frame.norm[5];
    g_lf_watch.norm6 = ctx->last_frame.norm[6];
    g_lf_watch.norm7 = ctx->last_frame.norm[7];

#ifdef LF_USE_STM32F4_HAL_PORT
    g_lf_watch.tim8_ccr3 = TIM8->CCR3;
    g_lf_watch.tim10_ccr1 = TIM10->CCR1;
    g_lf_watch.gpio_b_idr = GPIOB->IDR;
    g_lf_watch.gpio_b_odr = GPIOB->ODR;
    g_lf_watch.gpio_c_odr = GPIOC->ODR;
#endif
}

/**
 * @brief Update the Keil Watch debug snapshot with motor command data.
 * @param left_cmd    Left motor command.
 * @param right_cmd   Right motor command.
 * @param no_car_mode Non-zero if in no-car (bench) mode.
 */
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

/** @brief Convert an LF_AppState enum to a human-readable string tag. */
static const char *app_state_name(LF_AppState state)
{
    switch (state) {
    case LF_APP_STATE_BOOT: return "BOOT";
    case LF_APP_STATE_WAIT_START: return "WAIT_START";
    case LF_APP_STATE_CALIBRATING: return "CALIBRATING";
    case LF_APP_STATE_RUNNING: return "RUNNING";
    case LF_APP_STATE_RECOVERING: return "RECOVERING";
    case LF_APP_STATE_AVOID_PREP: return "AVOID_PREP";
    case LF_APP_STATE_AVOID_BYPASS: return "AVOID_BYPASS";
    case LF_APP_STATE_AVOID_REACQUIRE: return "AVOID_REACQUIRE";
    case LF_APP_STATE_FIXED_TURN_STOP: return "FIXED_STOP";
    case LF_APP_STATE_FIXED_TURN_SPIN: return "FIXED_SPIN";
    case LF_APP_STATE_FIXED_TURN_SETTLE: return "FIXED_SETTLE";
    case LF_APP_STATE_REORIENT_STOP: return "REORIENT_STOP";
    case LF_APP_STATE_REORIENT_SPIN: return "REORIENT_SPIN";
    case LF_APP_STATE_REORIENT_CONFIRM: return "REORIENT_CONFIRM";
    case LF_APP_STATE_STOPPED: return "STOPPED";
    case LF_APP_STATE_FAULT: return "FAULT";
    default: return "UNKNOWN";
    }
}

/** @brief Convert an LF_RadarObstacleState enum to a short string tag. */
static const char *radar_state_name(LF_RadarObstacleState state)
{
    switch (state) {
    case LF_RADAR_OBSTACLE_CLEAR: return "clear";
    case LF_RADAR_OBSTACLE_WARN: return "warn";
    case LF_RADAR_OBSTACLE_BLOCK: return "block";
    default: return "unknown";
    }
}

/** @brief Convert a WL_App_State enum to a short string tag. */
static const char *wl_state_name(WL_App_State state)
{
    switch (state) {
    case WL_APP_STATE_IDLE: return "IDLE";
    case WL_APP_STATE_READY: return "READY";
    case WL_APP_STATE_RUNNING: return "RUNNING";
    case WL_APP_STATE_FINISHED: return "FINISHED";
    case WL_APP_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

/**
 * @brief Initialize the debug monitor timer and motor command cache.
 */
void LF_DebugMonitor_Init(void)
{
    s_last_report_ms = LF_Platform_GetMillis();
    s_last_left_cmd = 0;
    s_last_right_cmd = 0;
}

/**
 * @brief Check whether debug monitor output is enabled.
 * @return true if enabled, false otherwise.
 */
bool LF_DebugMonitor_IsEnabled(void)
{
    return g_lf_debug_monitor_config.enabled;
}

/**
 * @brief Check whether no-car (bench) debug mode is active.
 * @return true if no-car mode is enabled, false otherwise.
 */
bool LF_DebugMonitor_IsNoCarMode(void)
{
    return g_lf_debug_monitor_config.no_car_mode;
}

/**
 * @brief Cache the most recent motor commands for debug reporting.
 * @param left_cmd  Last left motor command.
 * @param right_cmd Last right motor command.
 */
void LF_DebugMonitor_OnMotorCommand(int16_t left_cmd, int16_t right_cmd)
{
    s_last_left_cmd = left_cmd;
    s_last_right_cmd = right_cmd;
}

/**
 * @brief Retrieve the most recently cached motor commands.
 * @param[out] left_cmd  Receives the left motor command (may be NULL).
 * @param[out] right_cmd Receives the right motor command (may be NULL).
 */
void LF_DebugMonitor_GetLastMotorCommand(int16_t *left_cmd, int16_t *right_cmd)
{
    if (left_cmd != NULL) {
        *left_cmd = s_last_left_cmd;
    }
    if (right_cmd != NULL) {
        *right_cmd = s_last_right_cmd;
    }
}

/**
 * @brief Periodic debug output tick.
 *
 * Emits a single-line diagnostic via LF_Platform_DebugPrint when the
 * configured period has elapsed. Includes sensor, motor, radar, and
 * wireless state in a compact tagged format.
 */
void LF_DebugMonitor_Tick(void)
{
    static char line[512];
    static char raw_line[320];
    uint32_t now_ms;
    uint32_t period_ms;
    const LF_AppContext *ctx;
    const WL_LoRa_LinkStatus *link;
    const WL_App_Diag *wl_diag;
    int len;

    if (!g_lf_debug_monitor_config.enabled) {
        return;
    }

    now_ms = LF_Platform_GetMillis();
    period_ms = g_lf_debug_monitor_config.report_period_ms;
    if (period_ms == 0U) {
        period_ms = 200U;
    }
    if ((uint32_t)(now_ms - s_last_report_ms) < period_ms) {
        return;
    }
    s_last_report_ms = now_ms;

    ctx = LF_App_GetContext();
    if (ctx == NULL) {
        return;
    }

    link = g_lf_debug_monitor_config.report_wireless ? WL_LoRa_GetLinkStatus() : NULL;
    wl_diag = g_lf_debug_monitor_config.report_wireless ? WL_App_GetDiag() : NULL;

    len = snprintf(line,
                   sizeof(line),
                   "DBG t=%lu app=%s cal=%u line=%u pos=%ld sum=%lu peak=%u contrast=%u edge=%d motor=%d,%d radar=%s dist=%u route=%u cross=%u fixed=%d/%u seg=%d rec=%u wl=%s q=%u tx=%u/%u\n",
                   (unsigned long)now_ms,
                   app_state_name(ctx->state),
                   ctx->calibration_ok ? 1U : 0U,
                   ctx->last_frame.line_detected ? 1U : 0U,
                   (long)ctx->last_frame.position,
                   (unsigned long)ctx->last_frame.signal_sum,
                   (unsigned int)ctx->last_frame.peak_value,
                   (unsigned int)ctx->last_frame.contrast_value,
                   (int)ctx->last_frame.edge_hint,
                   (int)s_last_left_cmd,
                   (int)s_last_right_cmd,
                   g_lf_debug_monitor_config.report_radar ? radar_state_name(ctx->obstacle_state) : "off",
                   (unsigned int)(g_lf_debug_monitor_config.report_radar ? ctx->obstacle_distance_mm : 0U),
                   (unsigned int)ctx->route_phase,
                   (unsigned int)ctx->route_cross_count,
                   (int)ctx->fixed_turn_dir,
                   (unsigned int)ctx->fixed_turn_event,
                   (int)ctx->segment_type,
                   (unsigned int)ctx->recovery_count,
                   g_lf_debug_monitor_config.report_wireless ? wl_state_name(WL_App_GetState()) : "off",
                   (unsigned int)(link != NULL ? link->queue_depth : 0U),
                   (unsigned int)(link != NULL ? link->tx_success_count : 0U),
                   (unsigned int)(link != NULL ? link->tx_fail_count : 0U));
    if (len > 0) {
        LF_Platform_DebugPrint(line);
    }

    if (g_lf_debug_monitor_config.report_line_raw) {
        len = snprintf(raw_line,
                       sizeof(raw_line),
                       "DBGRAW raw=%u,%u,%u,%u,%u,%u,%u,%u norm=%u,%u,%u,%u,%u,%u,%u,%u wcp=%u/%u/%u ready=%u\n",
                       (unsigned int)ctx->last_frame.raw[0],
                       (unsigned int)ctx->last_frame.raw[1],
                       (unsigned int)ctx->last_frame.raw[2],
                       (unsigned int)ctx->last_frame.raw[3],
                       (unsigned int)ctx->last_frame.raw[4],
                       (unsigned int)ctx->last_frame.raw[5],
                       (unsigned int)ctx->last_frame.raw[6],
                       (unsigned int)ctx->last_frame.raw[7],
                       (unsigned int)ctx->last_frame.norm[0],
                       (unsigned int)ctx->last_frame.norm[1],
                       (unsigned int)ctx->last_frame.norm[2],
                       (unsigned int)ctx->last_frame.norm[3],
                       (unsigned int)ctx->last_frame.norm[4],
                       (unsigned int)ctx->last_frame.norm[5],
                       (unsigned int)ctx->last_frame.norm[6],
                       (unsigned int)ctx->last_frame.norm[7],
                       (unsigned int)(wl_diag != NULL ? wl_diag->checkpoint_enqueued_count : 0U),
                       (unsigned int)(wl_diag != NULL ? wl_diag->checkpoint_enqueue_fail_count : 0U),
                       (unsigned int)(wl_diag != NULL ? wl_diag->checkpoint_throttled_count : 0U),
                       (unsigned int)(g_lf_debug_monitor_config.report_wireless && Wireless_Hooks_IsReady() ? 1U : 0U));
        if (len > 0) {
            LF_Platform_DebugPrint(raw_line);
        }
    }
}
