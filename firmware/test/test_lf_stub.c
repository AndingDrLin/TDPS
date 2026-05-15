#include "lf_app.h"
#include "lf_debug_monitor.h"
#include "lf_platform.h"
#include "lf_radar.h"
#include "lf_sensor.h"
#include "lf_control.h"
#include "wireless_hooks.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int condition, const char *message)
{
    if (!condition) {
        printf("FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static void inject_simple_frame(uint16_t distance_mm, uint8_t target)
{
    uint8_t frame[8];

    frame[0] = 0xF4U;
    frame[1] = 0xF3U;
    frame[2] = 0xF2U;
    frame[3] = 0xF1U;
    frame[4] = (uint8_t)(distance_mm & 0xFFU);
    frame[5] = (uint8_t)(distance_mm >> 8);
    frame[6] = target;
    frame[7] = (uint8_t)(frame[4] ^ frame[5] ^ frame[6]);
    LF_PlatformStub_RadarInject(frame, (uint16_t)sizeof(frame));
}

static void inject_bad_simple_frame(void)
{
    const uint8_t frame[8] = {0xF4U, 0xF3U, 0xF2U, 0xF1U, 0x90U, 0x01U, 0x01U, 0x00U};
    LF_PlatformStub_RadarInject(frame, (uint16_t)sizeof(frame));
}

static void inject_ld2410s_minimal_frame(uint16_t distance_cm, uint8_t target_state)
{
    uint8_t frame[5];

    frame[0] = 0x6EU;
    frame[1] = target_state;
    frame[2] = (uint8_t)(distance_cm & 0xFFU);
    frame[3] = (uint8_t)(distance_cm >> 8);
    frame[4] = 0x62U;
    LF_PlatformStub_RadarInject(frame, (uint16_t)sizeof(frame));
}

static void inject_ld2410s_standard_frame(uint16_t distance_cm, uint8_t target_state)
{
    uint8_t frame[70] = {0};

    frame[0] = 0xF4U;
    frame[1] = 0xF3U;
    frame[2] = 0xF2U;
    frame[3] = 0xF1U;
    frame[4] = 0x46U;
    frame[5] = 0x00U;
    frame[6] = 0x01U;
    frame[7] = target_state;
    frame[8] = (uint8_t)(distance_cm & 0xFFU);
    frame[9] = (uint8_t)(distance_cm >> 8);
    frame[66] = 0xF8U;
    frame[67] = 0xF7U;
    frame[68] = 0xF6U;
    frame[69] = 0xF5U;
    LF_PlatformStub_RadarInject(frame, (uint16_t)sizeof(frame));
}

static void inject_block_frames(void)
{
    inject_ld2410s_minimal_frame(40U, 2U);
    inject_ld2410s_minimal_frame(40U, 2U);
    inject_ld2410s_minimal_frame(40U, 2U);
}

static int test_radar_simple_block(void)
{
    const LF_RadarState *radar;

    LF_Platform_BoardInit();
    LF_Radar_Init();
    inject_simple_frame(400U, 1U);
    inject_simple_frame(400U, 1U);
    inject_simple_frame(400U, 1U);
    LF_Radar_Tick(10U);
    radar = LF_Radar_GetState();

    return expect_true(radar->obstacle_state == LF_RADAR_OBSTACLE_BLOCK, "simple frame enters BLOCK") |
           expect_true(radar->distance_mm == 400U, "simple frame distance is mm") |
           expect_true(radar->target_state == 1U, "simple frame target state recorded") |
           expect_true(radar->frame_type == LF_RADAR_FRAME_SIMPLE, "simple frame type recorded");
}

static int test_radar_minimal_clear_state(void)
{
    const LF_RadarState *radar;

    LF_Platform_BoardInit();
    LF_Radar_Init();
    inject_ld2410s_minimal_frame(10U, 1U);
    inject_ld2410s_minimal_frame(10U, 1U);
    inject_ld2410s_minimal_frame(10U, 1U);
    LF_Radar_Tick(10U);
    radar = LF_Radar_GetState();

    return expect_true(radar->obstacle_state == LF_RADAR_OBSTACLE_CLEAR, "target_state 1 stays CLEAR") |
           expect_true(!radar->has_target, "target_state 1 is not a target") |
           expect_true(radar->distance_mm == 100U, "minimal frame converts cm to mm") |
           expect_true(radar->frame_type == LF_RADAR_FRAME_LD2410S_MINIMAL, "minimal frame type recorded");
}

static int test_radar_minimal_block(void)
{
    const LF_RadarState *radar;

    LF_Platform_BoardInit();
    LF_Radar_Init();
    inject_block_frames();
    LF_Radar_Tick(10U);
    radar = LF_Radar_GetState();

    return expect_true(radar->obstacle_state == LF_RADAR_OBSTACLE_BLOCK, "minimal frame enters BLOCK") |
           expect_true(radar->has_target, "target_state 2 is target") |
           expect_true(radar->distance_mm == 400U, "minimal block distance converted");
}

static int test_radar_standard_frame(void)
{
    const LF_RadarState *radar;
    uint32_t i;

    LF_Platform_BoardInit();
    LF_Radar_Init();
    inject_ld2410s_standard_frame(40U, 2U);
    inject_ld2410s_standard_frame(40U, 2U);
    inject_ld2410s_standard_frame(40U, 2U);
    for (i = 0U; i < 12U; ++i) {
        LF_Radar_Tick(10U + i);
    }
    radar = LF_Radar_GetState();

    return expect_true(radar->obstacle_state == LF_RADAR_OBSTACLE_BLOCK, "standard frame enters BLOCK") |
           expect_true(radar->frame_type == LF_RADAR_FRAME_LD2410S_STANDARD, "standard frame type recorded") |
           expect_true(radar->distance_mm == 400U, "standard frame distance converted");
}

static int test_radar_bad_checksum(void)
{
    const LF_RadarState *radar;

    LF_Platform_BoardInit();
    LF_Radar_Init();
    inject_bad_simple_frame();
    LF_Radar_Tick(10U);
    radar = LF_Radar_GetState();

    return expect_true(radar->parse_error_count == 1U, "bad checksum increments parse errors") |
           expect_true(!radar->frame_valid, "bad checksum does not validate frame");
}

static int test_radar_timeout_clears_block(void)
{
    const LF_RadarState *radar;

    LF_Platform_BoardInit();
    LF_Radar_Init();
    inject_block_frames();
    LF_Radar_Tick(10U);
    radar = LF_Radar_GetState();
    if (expect_true(radar->obstacle_state == LF_RADAR_OBSTACLE_BLOCK, "radar starts BLOCK before timeout")) {
        return 1;
    }

    LF_Radar_Tick(10U + g_lf_config.radar_frame_timeout_ms + 1U);
    radar = LF_Radar_GetState();
    return expect_true(radar->obstacle_state == LF_RADAR_OBSTACLE_CLEAR, "radar timeout clears BLOCK") |
           expect_true(!radar->frame_valid, "radar timeout invalidates frame");
}

static int test_sensor_weighted_position_and_edge_hint(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {0U, 0U, 0U, 0U, 4095U, 4095U, 4095U, 4095U};
    LF_SensorFrame frame;

    LF_Platform_BoardInit();
    LF_Sensor_Init();
    LF_Sensor_StartCalibration();
    LF_PlatformStub_SetLineSensorRaw(raw);
    LF_Sensor_ReadFrame(&frame);
    LF_PlatformStub_ClearLineSensorRaw();

    return expect_true(frame.line_detected, "sensor detects right-side line") |
           expect_true(frame.position == 1000, "sensor weighted position matches right-side weights") |
           expect_true(frame.edge_hint == 1, "sensor edge hint points right");
}

static int test_sensor_degraded_calibration_masks_bad_channels(void)
{
    const uint16_t low[LF_SENSOR_COUNT] = {900U, 900U, 900U, 900U, 900U, 900U, 900U, 900U};
    const uint16_t high[LF_SENSOR_COUNT] = {2500U, 2500U, 2500U, 2500U, 2500U, 900U, 900U, 900U};
    const uint16_t bad_only_high[LF_SENSOR_COUNT] = {900U, 900U, 900U, 900U, 900U, 4095U, 4095U, 4095U};
    const LF_SensorCalibration *cal;
    LF_SensorFrame frame;
    int failures = 0;

    LF_Platform_BoardInit();
    LF_Sensor_Init();
    LF_Sensor_StartCalibration();
    LF_PlatformStub_SetLineSensorRaw(low);
    LF_Sensor_UpdateCalibration();
    LF_PlatformStub_SetLineSensorRaw(high);
    LF_Sensor_UpdateCalibration();
    LF_Sensor_EndCalibration();

    cal = LF_Sensor_GetCalibration();
    failures += expect_true(cal->calibrated, "partial bad calibration remains usable");
    failures += expect_true(cal->status == LF_SENSOR_CAL_DEGRADED, "partial bad calibration is degraded");
    failures += expect_true(cal->valid_count == 5U, "degraded calibration counts valid sensors");
    failures += expect_true(cal->bad_mask == 0xE0U, "degraded calibration records bad channels");

    LF_PlatformStub_SetLineSensorRaw(bad_only_high);
    LF_Sensor_ReadFrame(&frame);
    failures += expect_true(frame.filtered_u16[5] == 0U && frame.filtered_u16[6] == 0U && frame.filtered_u16[7] == 0U,
                            "bad channels are masked from processed frame");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_pid_output_slew_limit(void)
{
    LF_PIDState pid;
    int16_t first;
    int16_t second;

    LF_Control_ResetPid(&pid);
    first = LF_Control_UpdatePid(2000.0f, 0.01f, &pid);
    second = LF_Control_UpdatePid(2000.0f, 0.01f, &pid);

    return expect_true(first <= g_lf_config.max_output_delta_per_tick,
                       "pid first output obeys slew limit") |
           expect_true((second - first) <= g_lf_config.max_output_delta_per_tick,
                       "pid output delta obeys slew limit");
}

static void run_app_for(uint32_t ms)
{
    uint32_t i;

    for (i = 0U; i < ms; ++i) {
        LF_App_RunStep();
        Wireless_Hooks_Tick();
        LF_DebugMonitor_Tick();
        LF_Platform_DelayMs(1U);
    }
}

static void run_app_step_after(uint32_t delay_ms)
{
    LF_Platform_DelayMs(delay_ms);
    LF_App_RunStep();
    Wireless_Hooks_Tick();
    LF_DebugMonitor_Tick();
}

static void set_center_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 2500U, 3300U, 3300U, 2500U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static int test_app_degraded_calibration_runs_limited(void)
{
    const uint16_t low[LF_SENSOR_COUNT] = {900U, 900U, 900U, 900U, 900U, 900U, 900U, 900U};
    const uint16_t high[LF_SENSOR_COUNT] = {2500U, 2500U, 2500U, 2500U, 2500U, 900U, 900U, 900U};
    int16_t left;
    int16_t right;
    const LF_AppContext *ctx;
    int failures = 0;

    LF_Platform_BoardInit();
    LF_DebugMonitor_Init();
    g_lf_debug_monitor_config.enabled = false;
    g_lf_debug_monitor_config.no_car_mode = true;
    (void)Wireless_Hooks_Init();
    LF_App_Init();

    run_app_for(1300U);
    LF_PlatformStub_SetLineSensorRaw(low);
    run_app_for(100U);
    LF_PlatformStub_SetLineSensorRaw(high);
    run_app_for(g_lf_config.calibration_duration_ms);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING, "degraded calibration enters RUNNING");
    failures += expect_true(ctx->calibration_degraded, "app records degraded calibration");
    failures += expect_true(ctx->reason == LF_APP_REASON_CALIBRATION_DEGRADED, "degraded calibration reason recorded");

    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    failures += expect_true(left <= g_lf_config.sensor_degraded_max_speed && right <= g_lf_config.sensor_degraded_max_speed,
                            "degraded calibration limits motor speed");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_debug_monitor_raw_and_reason(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {1000U, 1000U, 1000U, 1000U, 1000U, 1000U, 1000U, 1000U};
    const LF_SensorCalibration *cal;
    const char *last;
    int failures = 0;

    LF_Platform_BoardInit();
    LF_DebugMonitor_Init();
    g_lf_debug_monitor_config.enabled = true;
    g_lf_debug_monitor_config.no_car_mode = true;
    g_lf_debug_monitor_config.report_line_raw = true;
    g_lf_debug_monitor_config.report_period_ms = 4500U;
    LF_PlatformStub_SetLineSensorRaw(raw);
    LF_App_Init();
    run_app_for(5000U);
    cal = LF_Sensor_GetCalibration();
    failures += expect_true(LF_App_GetContext()->state == LF_APP_STATE_FAULT, "constant raw enters calibration FAULT");
    failures += expect_true(LF_App_GetContext()->reason == LF_APP_REASON_CALIBRATION_FAILED,
                            "FAULT reason records calibration failure");
    failures += expect_true(cal->bad_mask == 0xFFU, "calibration bad mask covers all sensors");
    last = LF_PlatformStub_GetLastDebugLine();
    failures += expect_true(strstr(last, "DBGCAL") != NULL, "debug monitor emits calibration line");
    failures += expect_true(strstr(last, "bad=0xFF") != NULL, "debug monitor reports calibration bad mask");
    g_lf_debug_monitor_config.enabled = false;
    g_lf_debug_monitor_config.report_line_raw = false;
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_app_recovery_timeout(void)
{
    const uint16_t lost_raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 900U, 900U, 900U, 900U, 900U};
    const LF_AppContext *ctx;

    LF_Platform_BoardInit();
    LF_DebugMonitor_Init();
    g_lf_debug_monitor_config.enabled = false;
    g_lf_debug_monitor_config.no_car_mode = true;
    (void)Wireless_Hooks_Init();
    LF_App_Init();

    run_app_for(5000U);
    set_center_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    if (expect_true(ctx->state == LF_APP_STATE_RUNNING, "app reaches RUNNING before line loss")) {
        return 1;
    }

    LF_PlatformStub_SetLineSensorRaw(lost_raw);
    run_app_for(120U);
    ctx = LF_App_GetContext();
    if (expect_true(ctx->state == LF_APP_STATE_RECOVERING, "line loss enters RECOVERING")) {
        return 1;
    }

    run_app_step_after(g_lf_config.recover_timeout_ms + 10U);
    ctx = LF_App_GetContext();
    LF_PlatformStub_ClearLineSensorRaw();

    return expect_true(ctx->state == LF_APP_STATE_STOPPED, "recovery timeout enters STOPPED") |
           expect_true(ctx->reason == LF_APP_REASON_RECOVERY_TIMEOUT, "recovery timeout reason recorded");
}

static int test_app_avoidance(void)
{
    const LF_AppContext *ctx;

    LF_Platform_BoardInit();
    LF_DebugMonitor_Init();
    g_lf_debug_monitor_config.enabled = false;
    g_lf_debug_monitor_config.no_car_mode = true;
    (void)Wireless_Hooks_Init();
    LF_App_Init();

    run_app_for(5000U);
    set_center_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    if (expect_true(ctx->state == LF_APP_STATE_RUNNING, "app reaches RUNNING before obstacle")) {
        return 1;
    }

    inject_block_frames();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    if (expect_true(ctx->state == LF_APP_STATE_AVOID_PREP, "BLOCK enters AVOID_PREP")) {
        return 1;
    }

    run_app_for(150U);
    ctx = LF_App_GetContext();
    if (expect_true(ctx->state == LF_APP_STATE_AVOID_TURN_OUT, "avoid prep enters turn out")) {
        return 1;
    }

    run_app_step_after(360U);
    run_app_step_after(900U);
    run_app_step_after(360U);
    ctx = LF_App_GetContext();
    if (ctx->state == LF_APP_STATE_AVOID_REACQUIRE) {
        run_app_for(50U);
    }
    ctx = LF_App_GetContext();

    return expect_true(ctx->state == LF_APP_STATE_RUNNING, "avoidance reacquires line and returns RUNNING");
}

int main(void)
{
    int failures = 0;

    failures += test_radar_simple_block();
    failures += test_radar_minimal_clear_state();
    failures += test_radar_minimal_block();
    failures += test_radar_standard_frame();
    failures += test_radar_bad_checksum();
    failures += test_radar_timeout_clears_block();
    failures += test_sensor_weighted_position_and_edge_hint();
    failures += test_sensor_degraded_calibration_masks_bad_channels();
    failures += test_pid_output_slew_limit();
    failures += test_app_degraded_calibration_runs_limited();
    failures += test_debug_monitor_raw_and_reason();
    failures += test_app_recovery_timeout();
    failures += test_app_avoidance();

    if (failures != 0) {
        printf("test_lf failed: %d\n", failures);
        return 1;
    }

    printf("test_lf passed\n");
    return 0;
}
