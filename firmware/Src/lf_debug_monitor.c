#include "lf_debug_monitor.h"

#include <stdio.h>

#include "lf_app.h"
#include "lf_platform.h"
#include "lf_sensor.h"
#include "wireless_hooks.h"
#include "wl_app.h"
#include "wl_lora.h"

#ifndef TDPS_NO_CAR_MODE
#define TDPS_NO_CAR_MODE 1
#endif

#ifndef TDPS_DEBUG_MONITOR_ENABLE
#define TDPS_DEBUG_MONITOR_ENABLE TDPS_NO_CAR_MODE
#endif

LF_DebugMonitorConfig g_lf_debug_monitor_config = {
    .enabled = (TDPS_DEBUG_MONITOR_ENABLE != 0),
    .no_car_mode = (TDPS_NO_CAR_MODE != 0),
    .report_period_ms = 200U,
    .report_line_raw = false,
    .report_wireless = true,
    .report_radar = true,
};

static uint32_t s_last_report_ms;
static int16_t s_last_left_cmd;
static int16_t s_last_right_cmd;

static const char *app_state_name(LF_AppState state)
{
    switch (state) {
    case LF_APP_STATE_BOOT:
        return "BOOT";
    case LF_APP_STATE_WAIT_START:
        return "WAIT_START";
    case LF_APP_STATE_CALIBRATING:
        return "CALIBRATING";
    case LF_APP_STATE_RUNNING:
        return "RUNNING";
    case LF_APP_STATE_RECOVERING:
        return "RECOVERING";
    case LF_APP_STATE_AVOID_PREP:
        return "AVOID_PREP";
    case LF_APP_STATE_AVOID_TURN_OUT:
        return "AVOID_TURN_OUT";
    case LF_APP_STATE_AVOID_BYPASS:
        return "AVOID_BYPASS";
    case LF_APP_STATE_AVOID_TURN_IN:
        return "AVOID_TURN_IN";
    case LF_APP_STATE_AVOID_REACQUIRE:
        return "AVOID_REACQUIRE";
    case LF_APP_STATE_STOPPED:
        return "STOPPED";
    case LF_APP_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

static const char *radar_state_name(LF_RadarObstacleState state)
{
    switch (state) {
    case LF_RADAR_OBSTACLE_CLEAR:
        return "clear";
    case LF_RADAR_OBSTACLE_WARN:
        return "warn";
    case LF_RADAR_OBSTACLE_BLOCK:
        return "block";
    default:
        return "unknown";
    }
}

static const char *wl_state_name(WL_App_State state)
{
    switch (state) {
    case WL_APP_STATE_IDLE:
        return "IDLE";
    case WL_APP_STATE_READY:
        return "READY";
    case WL_APP_STATE_RUNNING:
        return "RUNNING";
    case WL_APP_STATE_FINISHED:
        return "FINISHED";
    case WL_APP_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

static const char *reason_name(LF_AppReason reason)
{
    switch (reason) {
    case LF_APP_REASON_NONE:
        return "none";
    case LF_APP_REASON_WAIT_START:
        return "wait_start";
    case LF_APP_REASON_CALIBRATION_STARTED:
        return "cal_start";
    case LF_APP_REASON_CALIBRATION_FAILED:
        return "cal_failed";
    case LF_APP_REASON_CALIBRATION_DEGRADED:
        return "cal_degraded";
    case LF_APP_REASON_CALIBRATION_DONE:
        return "cal_done";
    case LF_APP_REASON_LINE_LOST:
        return "line_lost";
    case LF_APP_REASON_LINE_RECOVERED:
        return "line_recovered";
    case LF_APP_REASON_RECOVERY_TIMEOUT:
        return "recovery_timeout";
    case LF_APP_REASON_RADAR_BLOCK:
        return "radar_block";
    case LF_APP_REASON_RADAR_CLEAR:
        return "radar_clear";
    case LF_APP_REASON_AVOID_STARTED:
        return "avoid_started";
    case LF_APP_REASON_AVOID_RETRY:
        return "avoid_retry";
    case LF_APP_REASON_AVOID_FAILED:
        return "avoid_failed";
    case LF_APP_REASON_AVOID_COMPLETED:
        return "avoid_completed";
    case LF_APP_REASON_FAULT_FALLBACK:
        return "fault_fallback";
    default:
        return "unknown";
    }
}

static const char *radar_frame_type_name(LF_RadarFrameType type)
{
    switch (type) {
    case LF_RADAR_FRAME_NONE:
        return "none";
    case LF_RADAR_FRAME_SIMPLE:
        return "simple";
    case LF_RADAR_FRAME_LD2410S_MINIMAL:
        return "minimal";
    case LF_RADAR_FRAME_LD2410S_STANDARD:
        return "standard";
    default:
        return "unknown";
    }
}

void LF_DebugMonitor_Init(void)
{
    s_last_report_ms = LF_Platform_GetMillis();
    s_last_left_cmd = 0;
    s_last_right_cmd = 0;
}

bool LF_DebugMonitor_IsEnabled(void)
{
    return g_lf_debug_monitor_config.enabled;
}

bool LF_DebugMonitor_IsNoCarMode(void)
{
    return g_lf_debug_monitor_config.no_car_mode;
}

void LF_DebugMonitor_OnMotorCommand(int16_t left_cmd, int16_t right_cmd)
{
    s_last_left_cmd = left_cmd;
    s_last_right_cmd = right_cmd;
}

void LF_DebugMonitor_GetLastMotorCommand(int16_t *left_cmd, int16_t *right_cmd)
{
    if (left_cmd != NULL) {
        *left_cmd = s_last_left_cmd;
    }
    if (right_cmd != NULL) {
        *right_cmd = s_last_right_cmd;
    }
}

void LF_DebugMonitor_Tick(void)
{
    static char line[512];
    static char raw_line[384];
    static char cal_line[384];
    int len;
    uint32_t now_ms;
    uint32_t period_ms;
    const LF_AppContext *ctx;
    const WL_LoRa_LinkStatus *link;
    const WL_App_Diag *wl_diag;

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
                   "DBG t=%lu mode=%s app=%s reason=%s cal=%u line=%u pos=%ld sum=%lu peak=%u peakv=%u conf=%u edge=%d motor_l=%d motor_r=%d radar=%s rfv=%u rhas=%u rtype=%s rtgt=%u rfc=%lu rage=%lu dist=%u rerr=%lu wready=%u wl=%s wcp=%u wcpf=%u wcpt=%u lq=%u ldrop=%u lsucc=%u lfail=%u lretry=%u lack=%u lerr=%d\n",
                   (unsigned long)now_ms,
                   g_lf_debug_monitor_config.no_car_mode ? "no_car" : "run",
                   app_state_name(ctx->state),
                   reason_name(ctx->reason),
                   ctx->calibration_ok ? 1U : 0U,
                   ctx->last_frame.line_detected ? 1U : 0U,
                   (long)ctx->last_frame.position,
                   (unsigned long)ctx->last_frame.signal_sum,
                   (unsigned int)ctx->last_frame.peak_index,
                   (unsigned int)ctx->last_frame.peak_value,
                   (unsigned int)(ctx->last_frame.line_confidence * 1000.0f),
                   (int)ctx->last_frame.edge_hint,
                   (int)s_last_left_cmd,
                   (int)s_last_right_cmd,
                   g_lf_debug_monitor_config.report_radar ? radar_state_name(ctx->obstacle_state) : "off",
                   (unsigned int)(g_lf_debug_monitor_config.report_radar && ctx->radar_frame_valid ? 1U : 0U),
                   (unsigned int)(g_lf_debug_monitor_config.report_radar && ctx->radar_has_target ? 1U : 0U),
                   g_lf_debug_monitor_config.report_radar ? radar_frame_type_name(ctx->radar_frame_type) : "off",
                   (unsigned int)(g_lf_debug_monitor_config.report_radar ? ctx->radar_target_state : 0U),
                   (unsigned long)(g_lf_debug_monitor_config.report_radar ? ctx->radar_frame_count : 0U),
                   (unsigned long)(g_lf_debug_monitor_config.report_radar ? ctx->radar_frame_age_ms : 0U),
                   (unsigned int)(g_lf_debug_monitor_config.report_radar ? ctx->obstacle_distance_mm : 0U),
                   (unsigned long)(g_lf_debug_monitor_config.report_radar ? ctx->radar_parse_error_count : 0U),
                   (unsigned int)(g_lf_debug_monitor_config.report_wireless && Wireless_Hooks_IsReady() ? 1U : 0U),
                   g_lf_debug_monitor_config.report_wireless ? wl_state_name(WL_App_GetState()) : "off",
                   (unsigned int)(wl_diag != NULL ? wl_diag->checkpoint_enqueued_count : 0U),
                   (unsigned int)(wl_diag != NULL ? wl_diag->checkpoint_enqueue_fail_count : 0U),
                   (unsigned int)(wl_diag != NULL ? wl_diag->checkpoint_throttled_count : 0U),
                   (unsigned int)(link != NULL ? link->queue_depth : 0U),
                   (unsigned int)(link != NULL ? link->queue_dropped : 0U),
                   (unsigned int)(link != NULL ? link->tx_success_count : 0U),
                   (unsigned int)(link != NULL ? link->tx_fail_count : 0U),
                   (unsigned int)(link != NULL ? link->retry_count : 0U),
                   (unsigned int)((link != NULL && link->waiting_ack) ? 1U : 0U),
                   (int)(link != NULL ? link->last_error : WL_LORA_OK));

    if (len > 0) {
        LF_Platform_DebugPrint(line);
    }

    if (g_lf_debug_monitor_config.report_line_raw) {
        const LF_SensorCalibration *cal = LF_Sensor_GetCalibration();
        len = snprintf(raw_line,
                       sizeof(raw_line),
                       "DBGRAW t=%lu raw=%u,%u,%u,%u,%u,%u,%u,%u norm=%u,%u,%u,%u,%u,%u,%u,%u filt=%u,%u,%u,%u,%u,%u,%u,%u\n",
                       (unsigned long)now_ms,
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
                       (unsigned int)ctx->last_frame.filtered_u16[0],
                       (unsigned int)ctx->last_frame.filtered_u16[1],
                       (unsigned int)ctx->last_frame.filtered_u16[2],
                       (unsigned int)ctx->last_frame.filtered_u16[3],
                       (unsigned int)ctx->last_frame.filtered_u16[4],
                       (unsigned int)ctx->last_frame.filtered_u16[5],
                       (unsigned int)ctx->last_frame.filtered_u16[6],
                       (unsigned int)ctx->last_frame.filtered_u16[7]);
        if (len > 0) {
            LF_Platform_DebugPrint(raw_line);
        }
        len = snprintf(cal_line,
                       sizeof(cal_line),
                       "DBGCAL t=%lu ok=%u status=%u valid=%u bad=0x%02X min=%u,%u,%u,%u,%u,%u,%u,%u max=%u,%u,%u,%u,%u,%u,%u,%u\n",
                       (unsigned long)now_ms,
                       cal->calibrated ? 1U : 0U,
                       (unsigned int)cal->status,
                       (unsigned int)cal->valid_count,
                       (unsigned int)cal->bad_mask,
                       (unsigned int)cal->min_raw[0],
                       (unsigned int)cal->min_raw[1],
                       (unsigned int)cal->min_raw[2],
                       (unsigned int)cal->min_raw[3],
                       (unsigned int)cal->min_raw[4],
                       (unsigned int)cal->min_raw[5],
                       (unsigned int)cal->min_raw[6],
                       (unsigned int)cal->min_raw[7],
                       (unsigned int)cal->max_raw[0],
                       (unsigned int)cal->max_raw[1],
                       (unsigned int)cal->max_raw[2],
                       (unsigned int)cal->max_raw[3],
                       (unsigned int)cal->max_raw[4],
                       (unsigned int)cal->max_raw[5],
                       (unsigned int)cal->max_raw[6],
                       (unsigned int)cal->max_raw[7]);
        if (len > 0) {
            LF_Platform_DebugPrint(cal_line);
        }
    }
}
