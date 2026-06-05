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
    uint8_t frame[80] = {0};
    uint8_t i;

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
    for (i = 0U; i < LF_RADAR_GATE_COUNT; ++i) {
        uint32_t energy = (uint32_t)(100U + i * 10U);
        uint8_t offset = (uint8_t)(12U + i * 4U);
        frame[offset] = (uint8_t)(energy & 0xFFU);
        frame[offset + 1U] = (uint8_t)((energy >> 8) & 0xFFU);
        frame[offset + 2U] = (uint8_t)((energy >> 16) & 0xFFU);
        frame[offset + 3U] = (uint8_t)((energy >> 24) & 0xFFU);
    }
    frame[76] = 0xF8U;
    frame[77] = 0xF7U;
    frame[78] = 0xF6U;
    frame[79] = 0xF5U;
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
           expect_true(radar->distance_mm == 400U, "standard frame distance converted") |
           expect_true(radar->gate_energy[0] == 100U, "standard frame gate 0 energy parsed") |
           expect_true(radar->gate_energy[15] == 250U, "standard frame gate 15 energy parsed") |
           expect_true(radar->strongest_gate == 15U, "standard frame strongest gate parsed");
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
    LF_Sensor_ReadFrame(&frame);  /* prime median history */
    LF_Sensor_ReadFrame(&frame);  /* flush median */
    LF_Sensor_ReadFrame(&frame);  /* stable output */
    LF_PlatformStub_ClearLineSensorRaw();

    return expect_true(frame.line_detected, "sensor detects right-side line") |
           expect_true(frame.position == 1500, "sensor weighted position matches right-side weights") |
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

static int test_sensor_isolated_edge_noise_ignored_for_position(void)
{
    const uint16_t left_edge_noise[LF_SENSOR_COUNT] = {4095U, 0U, 0U, 4095U, 4095U, 0U, 0U, 0U};
    const uint16_t right_edge_noise[LF_SENSOR_COUNT] = {0U, 0U, 0U, 4095U, 4095U, 0U, 0U, 4095U};
    LF_SensorFrame frame;
    int failures = 0;

    LF_Platform_BoardInit();
    g_lf_config.sensor_input_mode = LF_SENSOR_INPUT_UART_PROTOCOL;
    g_lf_config.sensor_use_dynamic_calibration = false;
    g_lf_config.sensor_fast_calibration = true;
    g_lf_config.sensor_invert_polarity = false;
    g_lf_config.sensor_filter_alpha = 1.0f;
    g_lf_config.sensor_edge_noise_reject_enable = true;
    g_lf_config.sensor_edge_noise_neighbor_threshold = 260U;
    g_lf_config.line_detect_min_sum = 780U;
    g_lf_config.line_detect_min_peak = 260U;
    g_lf_config.line_detect_min_contrast = 80U;
    g_lf_config.sensor_weights[0] = -1750;
    g_lf_config.sensor_weights[1] = -1250;
    g_lf_config.sensor_weights[2] = -750;
    g_lf_config.sensor_weights[3] = -250;
    g_lf_config.sensor_weights[4] = 250;
    g_lf_config.sensor_weights[5] = 750;
    g_lf_config.sensor_weights[6] = 1250;
    g_lf_config.sensor_weights[7] = 1750;
    LF_Sensor_Init();
    LF_Sensor_StartCalibration();

    LF_PlatformStub_SetLineSensorRaw(left_edge_noise);
    LF_Sensor_ReadFrame(&frame);
    failures += expect_true(frame.line_detected, "left edge noise frame still detects center line");
    failures += expect_true(frame.filtered_u16[0] >= g_lf_config.line_detect_min_peak,
                            "left edge raw filtered value remains observable");
    failures += expect_true(frame.position == 0, "isolated left edge noise does not pull position");
    failures += expect_true(frame.edge_hint == 0, "isolated left edge noise does not set edge hint");

    LF_Sensor_Init();
    LF_Sensor_StartCalibration();
    LF_PlatformStub_SetLineSensorRaw(right_edge_noise);
    LF_Sensor_ReadFrame(&frame);
    failures += expect_true(frame.line_detected, "right edge noise frame still detects center line");
    failures += expect_true(frame.filtered_u16[7] >= g_lf_config.line_detect_min_peak,
                            "right edge raw filtered value remains observable");
    failures += expect_true(frame.position == 0, "isolated right edge noise does not pull position");
    failures += expect_true(frame.edge_hint == 0, "isolated right edge noise does not set edge hint");
    g_lf_config.sensor_input_mode = LF_SENSOR_INPUT_UART_PROTOCOL;
    g_lf_config.sensor_use_dynamic_calibration = true;
    g_lf_config.sensor_fast_calibration = false;
    g_lf_config.sensor_invert_polarity = false;
    g_lf_config.sensor_filter_alpha = 0.35f;
    g_lf_config.line_detect_min_sum = 780U;
    g_lf_config.line_detect_min_peak = 260U;
    g_lf_config.line_detect_min_contrast = 120U;
    g_lf_config.sensor_weights[0] = -1750;
    g_lf_config.sensor_weights[1] = -1250;
    g_lf_config.sensor_weights[2] = -750;
    g_lf_config.sensor_weights[3] = -250;
    g_lf_config.sensor_weights[4] = 250;
    g_lf_config.sensor_weights[5] = 750;
    g_lf_config.sensor_weights[6] = 1250;
    g_lf_config.sensor_weights[7] = 1750;
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

    if (g_lf_config.max_output_delta_per_tick == 0) return 0;
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

static void set_fork_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {2800U, 2800U, 2500U, 3300U, 3300U, 2500U, 2800U, 2800U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_branch_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {3300U, 3300U, 2500U, 900U, 900U, 900U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_right_branch_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 900U, 900U, 2500U, 3300U, 3300U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_far_right_entry_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 3300U, 3300U, 3300U, 3300U, 3300U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_wide_center_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {1200U, 2200U, 3300U, 4095U, 4095U, 3300U, 2200U, 1200U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_dirty_straight_noise(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {0U, 1230U, 1230U, 1230U, 1230U, 1230U, 1230U, 0U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_wide_biased_straight_noise(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 2800U, 3300U, 3300U, 3300U, 2800U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_slight_right_center_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 2200U, 3100U, 3300U, 2600U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_arrow_right_disturbance_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 3300U, 3300U, 3300U, 3300U, 3300U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_right_angle_ch1_to_ch6_on_ch8_dark(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {3300U, 3300U, 3300U, 3300U, 3300U, 3300U, 3300U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_right_angle_ch1_to_ch6_on_ch7_ch8_dark(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {3300U, 3300U, 3300U, 3300U, 3300U, 3300U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_right_right_angle_ch3_to_ch8_on_ch1_dark(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 3300U, 3300U, 3300U, 3300U, 3300U, 3300U, 3300U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_all_on_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {3300U, 3300U, 3300U, 3300U, 3300U, 3300U, 3300U, 3300U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_right_t_side_full_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 3300U, 3300U, 3300U, 3300U, 3300U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_right_angle_filtered_residue(void)
{
    const uint16_t all_on[LF_SENSOR_COUNT] = {3300U, 3300U, 3300U, 3300U, 3300U, 3300U, 3300U, 3300U};
    LF_PlatformStub_SetLineSensorRaw(all_on);
}

static void set_start_box_offset_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 2800U, 3300U, 3300U, 3300U, 2800U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_only_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {3300U, 3300U, 2500U, 900U, 900U, 900U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_edge_three_lamps(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {3300U, 3300U, 3300U, 900U, 900U, 900U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_curve_arc_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 3300U, 3300U, 3300U, 900U, 900U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_side_wide_curve_arc_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {3300U, 3300U, 3300U, 3300U, 3300U, 900U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_left_right_angle_inverse_ch8_on_middle_dark(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 900U, 900U, 900U, 900U, 3300U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_all_dark_line(void)
{
    const uint16_t raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 900U, 900U, 900U, 900U, 900U};
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static int init_app_to_running_with_start_guard(bool start_guard_enable)
{
    LF_Platform_BoardInit();
    LF_DebugMonitor_Init();
    g_lf_debug_monitor_config.enabled = false;
    g_lf_debug_monitor_config.no_car_mode = true;
    g_lf_config.auto_start_delay_ms = 10U;
    g_lf_config.start_min_boot_delay_ms = 0U;
    g_lf_config.start_line_hold_ms = 0U;
    g_lf_config.calibration_duration_ms = 10U;
    g_lf_config.sensor_use_dynamic_calibration = false;
    g_lf_config.sensor_fast_calibration = true;
    g_lf_config.obstacle_avoid_enable = true;
    g_lf_config.fork_enable = true;
    g_lf_config.route_script_enable = false;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.start_straight_guard_enable = start_guard_enable;
    (void)Wireless_Hooks_Init();
    LF_App_Init();
    run_app_for(g_lf_config.auto_start_delay_ms + g_lf_config.calibration_duration_ms + 20U);
    set_center_line();
    run_app_step_after(10U);
    return expect_true(LF_App_GetContext()->state == LF_APP_STATE_RUNNING, "app reaches RUNNING");
}

static int init_app_to_running(void)
{
    return init_app_to_running_with_start_guard(false);
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

    run_app_for(g_lf_config.auto_start_delay_ms + g_lf_config.calibration_duration_ms + 20U);
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

static int test_single_edge_hint_noise_does_not_slow_to_curve_speed(void)
{
    const uint16_t edge_hint_noise[LF_SENSOR_COUNT] = {4095U, 0U, 0U, 4095U, 4095U, 0U, 0U, 0U};
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.curve_prepare_enable = true;
    g_lf_config.curve_prepare_speed = 40;
    g_lf_config.curve_prepare_error_threshold = 400;
    g_lf_config.curve_prepare_delta_threshold = 180;
    g_lf_config.curve_prepare_confirm_ticks = 1U;
    g_lf_config.adaptive_confidence_threshold = 0.20f;
    g_lf_config.sensor_edge_noise_reject_enable = true;
    g_lf_config.sensor_edge_noise_neighbor_threshold = 260U;
    g_lf_config.sensor_filter_alpha = 1.0f;
    set_center_line();
    run_app_for(40U);
    LF_PlatformStub_SetLineSensorRaw(edge_hint_noise);
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->current_target_speed == g_lf_config.base_speed,
                            "single isolated edge noise does not select curve speed");
    failures += expect_true(ctx->last_frame.edge_hint == 0,
                            "isolated edge noise is removed before edge hint calculation");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_offset_line_still_turns_with_deadband(void)
{
    const uint16_t far_left_line[LF_SENSOR_COUNT] = {4095U, 4095U, 3300U, 0U, 0U, 0U, 0U, 0U};
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.stable_direction_enable = true;
    g_lf_config.line_stability_enable = true;
    g_lf_config.control_error_deadband = 80;
    g_lf_config.control_error_soft_zone = 240;
    g_lf_config.max_output_delta_per_tick = 200;
    g_lf_config.sensor_filter_alpha = 1.0f;
    set_center_line();
    run_app_for(40U);
    LF_PlatformStub_SetLineSensorRaw(far_left_line);
    run_app_for(30U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    failures += expect_true(ctx->last_frame.line_detected, "offset line remains detected");
    failures += expect_true(ctx->last_frame.position < -400, "offset line keeps large position error");
    failures += expect_true(left > right + 40, "offset left line still commands left turn");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_wide_center_deadband_softens_motor_output(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.stable_direction_enable = true;
    g_lf_config.line_stability_enable = true;
    g_lf_config.control_error_deadband = 80;
    g_lf_config.control_error_soft_zone = 240;
    g_lf_config.max_output_delta_per_tick = 200;
    set_center_line();
    run_app_for(40U);
    set_wide_biased_straight_noise();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->last_frame.line_detected, "wide biased center frame remains detected");
    failures += expect_true(ctx->last_frame.active_count >= 3U, "wide biased center frame keeps wide-line coverage");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_start_straight_guard_limits_wide_black_turn(void)
{
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;
    int16_t diff;
    int16_t avg;
    int failures = 0;

    if (init_app_to_running_with_start_guard(true)) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.line_stability_enable = true;
    g_lf_config.stable_direction_enable = true;
    g_lf_config.sensor_filter_alpha = 1.0f;
    g_lf_config.base_speed = 200;
    g_lf_config.min_speed = 60;
    g_lf_config.kp = 0.18f;
    g_lf_config.kd = 0.0f;
    g_lf_config.max_correction = 300;
    g_lf_config.max_output_delta_per_tick = 300;
    g_lf_config.start_straight_guard_enable = true;
    g_lf_config.start_straight_guard_ms = 1200U;
    g_lf_config.start_straight_guard_speed = 120;
    g_lf_config.start_straight_guard_max_correction = 15;
    g_lf_config.start_straight_guard_active_count = 4U;
    g_lf_config.start_straight_guard_release_ticks = 4U;
    g_lf_config.start_straight_guard_release_active_count = 3U;
    g_lf_config.start_straight_guard_release_error = 220;

    set_start_box_offset_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    diff = (left > right) ? (int16_t)(left - right) : (int16_t)(right - left);
    avg = (int16_t)((left + right) / 2);

    failures += expect_true(ctx->last_frame.line_detected, "start box wide line remains detected");
    failures += expect_true(ctx->last_frame.active_count >= 4U, "start box frame has wide active coverage");
    failures += expect_true(diff <= 4,
                            "start guard drives straight on wide black");
    failures += expect_true(avg <= g_lf_config.start_straight_guard_speed + 5,
                            "start guard keeps slow forward speed");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_edge_realign_uses_small_motor_delta(void)
{
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;
    int16_t diff;
    int failures = 0;

    if (init_app_to_running_with_start_guard(true)) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.edge_realign_enable = true;
    g_lf_config.edge_realign_dir_sign = 1;
    g_lf_config.edge_realign_speed = 135;
    g_lf_config.edge_realign_delta = 35;
    g_lf_config.edge_realign_confirm_ticks = 1U;
    g_lf_config.max_motor_delta = 70;
    g_lf_config.sensor_filter_alpha = 1.0f;

    set_left_edge_three_lamps();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    diff = (left > right) ? (int16_t)(left - right) : (int16_t)(right - left);

    failures += expect_true(ctx->edge_realign_side == -1, "left edge lamps enter edge realign");
    failures += expect_true(diff <= g_lf_config.max_motor_delta, "edge realign obeys global small turn limit");
    failures += expect_true(diff > 0, "edge realign applies a small correction");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_curve_arc_uses_independent_small_delta(void)
{
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;
    int16_t diff;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.edge_realign_enable = true;
    g_lf_config.edge_realign_confirm_ticks = 1U;
    g_lf_config.curve_arc_enable = true;
    g_lf_config.curve_arc_speed = 115;
    g_lf_config.curve_arc_delta = 50;
    g_lf_config.curve_arc_confirm_ticks = 1U;
    g_lf_config.curve_arc_release_ticks = 2U;
    g_lf_config.max_motor_delta = 70;
    g_lf_config.sensor_filter_alpha = 1.0f;

    set_left_curve_arc_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    diff = (left > right) ? (int16_t)(left - right) : (int16_t)(right - left);

    failures += expect_true(ctx->curve_arc_side == -1, "left-biased middle lamps enter curve arc");
    failures += expect_true(ctx->edge_realign_side == 0, "curve arc does not use edge realign");
    failures += expect_true(diff > 0 && diff <= g_lf_config.max_motor_delta,
                            "curve arc applies bounded small motor delta");

    set_center_line();
    run_app_for(30U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->curve_arc_side == 0, "center lamps release curve arc");
    g_lf_config.curve_arc_enable = false;
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_side_wide_lamps_enter_curve_arc_before_edge_realign(void)
{
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;
    int16_t diff;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.edge_realign_enable = true;
    g_lf_config.edge_realign_confirm_ticks = 1U;
    g_lf_config.curve_arc_enable = true;
    g_lf_config.curve_arc_speed = 115;
    g_lf_config.curve_arc_delta = 110;
    g_lf_config.curve_arc_max_motor_delta = 130;
    g_lf_config.curve_arc_confirm_ticks = 1U;
    g_lf_config.max_motor_delta = 70;
    g_lf_config.line_detect_min_peak = 300U;
    g_lf_config.sensor_filter_alpha = 1.0f;

    set_left_side_wide_curve_arc_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    diff = (left > right) ? (int16_t)(left - right) : (int16_t)(right - left);
    failures += expect_true(ctx->curve_arc_side == -1, "left side-wide lamps enter curve arc");
    failures += expect_true(ctx->edge_realign_side == 0, "side-wide curve is not stolen by edge realign");
    failures += expect_true(diff > g_lf_config.max_motor_delta && diff <= 130,
                            "side-wide curve can turn harder than straight delta limit");
    g_lf_config.curve_arc_enable = false;
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_curve_arc_preserves_pid_state(void)
{
    const LF_AppContext *ctx;
    float saved_prev_error;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.curve_arc_enable = true;
    g_lf_config.curve_arc_speed = 80;
    g_lf_config.curve_arc_delta = 140;
    g_lf_config.curve_arc_max_motor_delta = 150;
    g_lf_config.curve_arc_confirm_ticks = 2U;
    g_lf_config.curve_arc_release_ticks = 4U;
    g_lf_config.sensor_filter_alpha = 1.0f;

    /* 建立 PID 状态 */
    set_center_line();
    run_app_for(20U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->pid.initialized == 1U, "PID initialized after straight");
    saved_prev_error = ctx->pid.prev_error;

    /* 进入弯道 */
    set_left_side_wide_curve_arc_line();
    run_app_for(5U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->curve_arc_side != 0, "entered curve arc");
    failures += expect_true(ctx->pid.initialized == 1U,
                            "PID still initialized during curve arc (not reset)");
    {
        float diff_err = ctx->pid.prev_error - saved_prev_error;
        if (diff_err < 0.0f) diff_err = -diff_err;
        failures += expect_true(diff_err < 0.5f,
                                "PID prev_error preserved during curve arc");
    }

    g_lf_config.curve_arc_enable = false;
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_straight_noise_keeps_base_speed(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.curve_prepare_enable = true;
    g_lf_config.curve_prepare_speed = 40;
    g_lf_config.straight_noise_reject_enable = true;
    g_lf_config.straight_noise_confirm_ticks = 1U;
    g_lf_config.straight_noise_active_count_threshold = 5U;
    g_lf_config.straight_noise_max_sum = 2200U;
    g_lf_config.straight_noise_max_position_error = 300;
    g_lf_config.straight_noise_max_position_delta = 300;
    set_center_line();
    run_app_for(40U);
    set_dirty_straight_noise();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->current_target_speed == g_lf_config.base_speed,
                            "straight noise keeps base speed");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_arrow_like_wide_frame_does_not_start_lead_without_entry(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = true;
    g_lf_config.lead_event_confirm_ticks = 1U;
    g_lf_config.lead_event_active_count_threshold = 5U;
    g_lf_config.lead_event_min_sum = 1800U;
    g_lf_config.lead_event_center_error_threshold = 350;
    g_lf_config.lead_event_entry_error_threshold = 650;
    set_wide_biased_straight_noise();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->lead_phase == (uint8_t)LF_LEAD_PHASE_IDLE,
                            "arrow-like wide frame without entry does not start lead phase");
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING,
                            "arrow-like wide frame without entry stays running");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_arrow_interference_drives_straight_on_low_speed_line(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = true;
    g_lf_config.reorient_enable = true;
    g_lf_config.curve_arc_enable = true;
    g_lf_config.line_stability_enable = true;
    g_lf_config.stable_direction_enable = true;
    g_lf_config.sensor_filter_alpha = 1.0f;
    g_lf_config.base_speed = 100;
    g_lf_config.seg_base_speed_straight = 100;
    g_lf_config.start_straight_guard_speed = 90;
    g_lf_config.interference_active_count_threshold = 5U;
    g_lf_config.interference_position_jump_threshold = 260;
    g_lf_config.interference_hold_ticks = 8U;
    g_lf_config.straight_noise_max_position_error = 250;
    g_lf_config.lead_event_confirm_ticks = 1U;
    g_lf_config.lead_event_active_count_threshold = 5U;
    g_lf_config.lead_event_min_sum = 1800U;
    g_lf_config.reorient_position_threshold = 300;

    set_slight_right_center_line();
    run_app_for(40U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->trusted_line_valid, "trusted straight line established before arrow interference");

    set_arrow_right_disturbance_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();

    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING, "arrow interference stays in RUNNING");
    failures += expect_true(ctx->lead_phase == (uint8_t)LF_LEAD_PHASE_IDLE,
                            "arrow interference does not start lead compensation");
    failures += expect_true(ctx->fork_detect_count == 0U, "arrow interference does not accumulate fork detection");

    run_app_for(20U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING,
                            "arrow interference tail stays in RUNNING without recovery/reorient");

    set_center_line();
    run_app_for(30U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->interference_hold_count == 0U,
                            "normal center line releases arrow interference hold");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int expect_opposite_equal_spin(const char *message)
{
    int16_t left;
    int16_t right;
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    return expect_true(left == (int16_t)(-right), message) |
           expect_true(left != 0 && right != 0, "reorient spin command is non-zero");
}

static int test_left_right_angle_reorient_accepts_ch8_dark(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.curve_arc_enable = true;
    g_lf_config.fixed_turn_enable = false;
    g_lf_config.reorient_enable = true;
    g_lf_config.right_angle_confirm_ticks = 1U;
    g_lf_config.reorient_approach_ms = 0U;
    g_lf_config.reorient_stop_ms = 0U;
    g_lf_config.reorient_min_spin_ms = 180U;
    g_lf_config.reorient_position_threshold = 520;
    g_lf_config.sensor_filter_alpha = 1.0f;

    set_left_right_angle_ch1_to_ch6_on_ch8_dark();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_REORIENT_SPIN,
                            "left right-angle with ch1..ch6 on and ch8 dark starts immediate reorient spin");
    failures += expect_true(ctx->reorient_spin_dir == -1,
                            "left right-angle with ch8 dark spins left");
    failures += expect_opposite_equal_spin("left right-angle with ch8 dark uses opposite equal wheel speeds");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_left_right_angle_reorient_accepts_ch7_ch8_dark(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.curve_arc_enable = true;
    g_lf_config.fixed_turn_enable = false;
    g_lf_config.reorient_enable = true;
    g_lf_config.right_angle_confirm_ticks = 1U;
    g_lf_config.reorient_approach_ms = 0U;
    g_lf_config.reorient_stop_ms = 0U;
    g_lf_config.reorient_min_spin_ms = 180U;
    g_lf_config.reorient_position_threshold = 520;
    g_lf_config.sensor_filter_alpha = 1.0f;

    set_left_right_angle_ch1_to_ch6_on_ch7_ch8_dark();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_REORIENT_SPIN,
                            "left right-angle with ch1..ch6 on and ch7/ch8 dark starts immediate reorient spin");
    failures += expect_true(ctx->reorient_spin_dir == -1,
                            "left right-angle with ch7/ch8 dark spins left");
    failures += expect_opposite_equal_spin("left right-angle with ch7/ch8 dark uses opposite equal wheel speeds");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_left_right_angle_uses_current_sample_not_filtered_residue(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.curve_arc_enable = true;
    g_lf_config.fixed_turn_enable = false;
    g_lf_config.reorient_enable = true;
    g_lf_config.right_angle_confirm_ticks = 1U;
    g_lf_config.reorient_approach_ms = 0U;
    g_lf_config.reorient_stop_ms = 0U;
    g_lf_config.reorient_min_spin_ms = 180U;
    g_lf_config.reorient_position_threshold = 520;
    g_lf_config.sensor_filter_alpha = 0.20f;

    set_left_right_angle_filtered_residue();
    run_app_for(40U);
    set_left_right_angle_ch1_to_ch6_on_ch8_dark();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->last_frame.norm[7] >= g_lf_config.line_detect_min_peak,
                            "median ch8 residue remains active");
    failures += expect_true(ctx->last_frame.filtered_u16[7] >= g_lf_config.line_detect_min_peak,
                            "filtered ch8 residue remains active");
    failures += expect_true(ctx->last_frame.instant_norm[7] < g_lf_config.line_detect_min_peak,
                            "current ch8 instant sample is dark");
    failures += expect_true(ctx->state == LF_APP_STATE_REORIENT_SPIN,
                            "left right-angle triggers from current sample despite median/filter residue");
    failures += expect_true(ctx->reorient_spin_dir == -1,
                            "filtered-residue left right-angle spins left");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_fixed_turn_overrides_existing_right_angle_reorient(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.route_script_enable = false;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.curve_arc_enable = true;
    g_lf_config.reorient_enable = true;
    g_lf_config.right_angle_confirm_ticks = 1U;
    g_lf_config.reorient_approach_ms = 0U;
    g_lf_config.reorient_stop_ms = 0U;
    g_lf_config.sensor_filter_alpha = 1.0f;

    set_left_right_angle_ch1_to_ch6_on_ch8_dark();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "fixed turn overrides existing low-success right-angle reorient");
    failures += expect_true(ctx->fixed_turn_dir == -1,
                            "left right-angle enters fixed left spin");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_fixed_turn_accepts_inverse_led_polarity_right_angle(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.route_script_enable = false;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.reorient_enable = false;
    g_lf_config.sensor_filter_alpha = 1.0f;

    set_left_right_angle_inverse_ch8_on_middle_dark();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "inverse LED polarity left right-angle starts fixed turn");
    failures += expect_true(ctx->fixed_turn_dir == -1,
                            "inverse LED polarity right-angle turns left");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_route_all_dark_t_turns_right_as_all_on(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.route_script_enable = true;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_90_ms_right = 60U;
    g_lf_config.fixed_turn_settle_ms = 10U;
    g_lf_config.route_event_confirm_ticks = 1U;
    g_lf_config.route_event_cooldown_ms = 0U;
    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.reorient_enable = false;
    g_lf_config.sensor_filter_alpha = 1.0f;

    ((LF_AppContext *)LF_App_GetContext())->route_phase = (uint8_t)LF_ROUTE_PHASE_WAIT_FIRST_T_RIGHT;
    set_all_dark_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "inverse LED polarity all-on T starts fixed right turn");
    failures += expect_true(ctx->fixed_turn_dir == +1,
                            "inverse LED polarity all-on T turns right");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_route_all_on_t_turns_right_without_arm_delay(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.route_script_enable = true;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_90_ms_right = 60U;
    g_lf_config.fixed_turn_settle_ms = 10U;
    g_lf_config.route_event_confirm_ticks = 1U;
    g_lf_config.route_event_cooldown_ms = 0U;
    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.reorient_enable = false;
    g_lf_config.sensor_filter_alpha = 1.0f;

    set_all_on_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "all-on T starts fixed right turn without arm delay");
    failures += expect_true(ctx->fixed_turn_dir == +1,
                            "all-on T turns clockwise/right");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_fixed_turn_right_runs_full_duration_even_when_center_seen(void)
{
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.route_script_enable = true;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_spin_speed = 180;
    g_lf_config.fixed_turn_90_ms_right = 120U;
    g_lf_config.fixed_turn_settle_ms = 20U;
    g_lf_config.route_event_confirm_ticks = 1U;
    g_lf_config.route_event_cooldown_ms = 0U;
    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = false;
    g_lf_config.reorient_enable = false;
    g_lf_config.sensor_filter_alpha = 1.0f;

    /* 测试直接定位到首个 T 口阶段，验证固定动作本身不会被中线提前结束。 */
    ((LF_AppContext *)LF_App_GetContext())->route_phase = (uint8_t)LF_ROUTE_PHASE_WAIT_FIRST_T_RIGHT;
    set_all_on_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "all-on first T starts fixed right turn");
    failures += expect_true(ctx->fixed_turn_dir == +1, "fixed first T turn direction is right");
    failures += expect_true(left == (int16_t)(-right) && left > 0,
                            "fixed right turn uses clockwise opposite spin");

    set_center_line();
    run_app_for(60U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "fixed turn ignores center line before duration elapses");

    run_app_for(100U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING,
                            "fixed turn returns RUNNING after duration and settle");
    failures += expect_true(ctx->route_phase == (uint8_t)LF_ROUTE_PHASE_COUNT_TWO_CROSSES,
                            "fixed first T completion enters cross-count phase");
    failures += expect_true(ctx->reason == LF_APP_REASON_FIXED_TURN_DONE,
                            "fixed turn completion records reason");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_route_initial_right_angle_uses_detected_right_direction(void)
{
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.route_script_enable = true;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_spin_speed = 180;
    g_lf_config.fixed_turn_90_ms_right = 60U;
    g_lf_config.fixed_turn_settle_ms = 10U;
    g_lf_config.route_event_confirm_ticks = 1U;
    g_lf_config.route_event_cooldown_ms = 0U;
    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.reorient_enable = false;
    g_lf_config.sensor_filter_alpha = 1.0f;

    ((LF_AppContext *)LF_App_GetContext())->route_phase = (uint8_t)LF_ROUTE_PHASE_WAIT_INITIAL_RIGHT_ANGLE;
    set_right_right_angle_ch3_to_ch8_on_ch1_dark();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "route initial right-angle starts fixed turn");
    failures += expect_true(ctx->fixed_turn_dir == +1,
                            "route initial right-angle keeps detected right direction");
    failures += expect_true(left == (int16_t)(-right) && left > 0,
                            "route initial right-angle spins clockwise");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_route_counts_two_all_on_crosses_then_next_left_turn(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.route_script_enable = true;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_spin_speed = 180;
    g_lf_config.fixed_turn_90_ms_left = 60U;
    g_lf_config.fixed_turn_settle_ms = 10U;
    g_lf_config.route_event_confirm_ticks = 1U;
    g_lf_config.route_event_cooldown_ms = 0U;
    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.reorient_enable = false;
    g_lf_config.sensor_filter_alpha = 1.0f;

    ((LF_AppContext *)LF_App_GetContext())->route_phase = (uint8_t)LF_ROUTE_PHASE_COUNT_TWO_CROSSES;
    ((LF_AppContext *)LF_App_GetContext())->route_cross_armed = true;
    set_all_on_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING,
                            "first all-on cross keeps driving straight");
    failures += expect_true(ctx->route_cross_count == 1U,
                            "first all-on cross increments route counter");
    failures += expect_true(ctx->route_phase == (uint8_t)LF_ROUTE_PHASE_COUNT_TWO_CROSSES,
                            "route waits for second all-on cross");

    set_center_line();
    run_app_step_after(10U);
    set_all_on_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING,
                            "second all-on cross also keeps driving straight");
    failures += expect_true(ctx->route_cross_count == 2U,
                            "second all-on cross increments route counter");
    failures += expect_true(ctx->route_phase == (uint8_t)LF_ROUTE_PHASE_WAIT_NEXT_LEFT_RIGHT_ANGLE,
                            "after two crosses route waits for next left right-angle");

    set_center_line();
    run_app_step_after(10U);
    set_left_right_angle_ch1_to_ch6_on_ch8_dark();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "next left right-angle starts fixed turn after two crosses");
    failures += expect_true(ctx->fixed_turn_dir == -1,
                            "next right-angle turns left");
    failures += expect_true(ctx->route_phase == (uint8_t)LF_ROUTE_PHASE_DONE,
                            "route script completes after next left right-angle");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_route_final_t_right_side_full_starts_right_turn(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.route_script_enable = true;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_90_ms_right = 60U;
    g_lf_config.fixed_turn_settle_ms = 10U;
    g_lf_config.route_event_confirm_ticks = 1U;
    g_lf_config.route_event_cooldown_ms = 0U;
    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.reorient_enable = false;
    g_lf_config.sensor_filter_alpha = 1.0f;

    ((LF_AppContext *)LF_App_GetContext())->route_phase = (uint8_t)LF_ROUTE_PHASE_WAIT_FINAL_T_RIGHT;
    set_right_t_side_full_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN,
                            "right-side-full final T starts fixed turn");
    failures += expect_true(ctx->fixed_turn_dir == +1,
                            "right-side-full final T turns right");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_biased_wide_noise_does_not_start_lead_event(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = true;
    g_lf_config.lead_event_confirm_ticks = 1U;
    g_lf_config.lead_event_active_count_threshold = 5U;
    g_lf_config.lead_event_min_sum = 1800U;
    g_lf_config.lead_event_center_error_threshold = 350;
    g_lf_config.lead_event_entry_error_threshold = 650;
    set_wide_biased_straight_noise();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->lead_phase == (uint8_t)LF_LEAD_PHASE_IDLE,
                            "biased wide straight noise does not start lead phase");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_lead_event_advances_before_turning(void)
{
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = true;
    g_lf_config.lead_event_confirm_ticks = 1U;
    g_lf_config.lead_advance_ticks = 10U;
    g_lf_config.lead_advance_speed = 44;
    g_lf_config.lead_turn_hold_ticks = 3U;
    g_lf_config.lead_turn_speed = 40;
    g_lf_config.lead_turn_delta = 70;
    g_lf_config.lead_entry_memory_ticks = 20U;
    g_lf_config.lead_event_active_count_threshold = 4U;
    g_lf_config.lead_event_min_sum = 1800U;
    g_lf_config.lead_event_center_error_threshold = 350;
    g_lf_config.lead_event_entry_error_threshold = 650;
    g_lf_config.sensor_filter_alpha = 1.0f;

    /* 用右侧入弯线运行 5 ticks，冲刷中值滤波器并触发 entry path → 启动 advance */
    set_far_right_entry_line();
    run_app_for(50U);
    /* 切换到宽中心线，验证仍在 advance 阶段（advance_ticks=10 > 5+1） */
    set_wide_center_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    failures += expect_true(ctx->lead_phase == (uint8_t)LF_LEAD_PHASE_ADVANCE,
                            "lead event starts advance phase");
    failures += expect_true(left == right, "lead advance drives straight before turning");

    /* 继续运行直到 advance 结束，进入 turn_hold */
    run_app_for(80U);
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING, "lead phase stays in RUNNING");
    failures += expect_true(left < right, "lead turn hold keeps right turn after advance");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_lead_event_without_entry_direction_does_not_random_turn(void)
{
    int16_t left;
    int16_t right;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = true;
    g_lf_config.lead_event_confirm_ticks = 1U;
    g_lf_config.lead_advance_ticks = 1U;
    g_lf_config.lead_advance_speed = 44;
    g_lf_config.lead_turn_hold_ticks = 3U;
    g_lf_config.lead_turn_speed = 40;
    g_lf_config.lead_turn_delta = 90;
    g_lf_config.lead_entry_memory_ticks = 20U;
    g_lf_config.lead_event_active_count_threshold = 5U;
    g_lf_config.lead_event_min_sum = 1800U;
    g_lf_config.lead_event_center_error_threshold = 350;
    g_lf_config.lead_event_entry_error_threshold = 1200;

    set_wide_center_line();
    run_app_step_after(10U);
    run_app_for(30U);
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);
    failures += expect_true(left == right, "lead event without entry direction does not force turn");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_line_loss_resets_lead_phase(void)
{
    const uint16_t lost_raw[LF_SENSOR_COUNT] = {900U, 900U, 900U, 900U, 900U, 900U, 900U, 900U};
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    g_lf_config.fork_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.lead_compensation_enable = true;
    g_lf_config.lead_event_confirm_ticks = 1U;
    g_lf_config.lead_advance_ticks = 20U;
    g_lf_config.lead_advance_speed = 44;
    g_lf_config.lead_event_active_count_threshold = 4U;
    g_lf_config.lead_event_min_sum = 1800U;
    g_lf_config.lead_event_center_error_threshold = 350;
    g_lf_config.lead_event_entry_error_threshold = 650;
    g_lf_config.sensor_filter_alpha = 1.0f;
    /* 冲刷中值滤波器 → 入口路径触发 → 启动 advance */
    set_far_right_entry_line();
    run_app_for(50U);
    set_wide_center_line();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->lead_phase == (uint8_t)LF_LEAD_PHASE_ADVANCE,
                            "lead phase starts before line loss");

    LF_PlatformStub_SetLineSensorRaw(lost_raw);
    run_app_for(120U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->lead_phase == (uint8_t)LF_LEAD_PHASE_IDLE,
                            "line loss resets lead phase");
    failures += expect_true(ctx->state == LF_APP_STATE_RECOVERING,
                            "line loss still enters recovery");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_fork_left_blocked_commits_right(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    set_fork_line();
    run_app_for(30U);
    inject_block_frames();
    run_app_for(30U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FORK_COMMIT_RIGHT,
                            "left blocked fork commits right instead of generic avoidance");
    failures += expect_true(ctx->fork_decision == +1, "left blocked fork records right decision");
    failures += expect_true(ctx->reason == LF_APP_REASON_FORK_LEFT_BLOCKED, "left blocked fork reason recorded");

    set_right_branch_line();
    run_app_for(g_lf_config.fork_commit_ms + g_lf_config.fork_reacquire_timeout_ms);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING, "right branch reacquires line and returns RUNNING");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_fork_left_clear_commits_left(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    inject_ld2410s_minimal_frame(150U, 1U);
    set_fork_line();
    run_app_for(g_lf_config.fork_sample_ms + 60U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FORK_COMMIT_LEFT,
                            "clear left fork commits left");
    failures += expect_true(ctx->fork_decision == -1, "clear left fork records left decision");
    failures += expect_true(ctx->reason == LF_APP_REASON_FORK_LEFT_CLEAR, "clear left fork reason recorded");

    set_left_branch_line();
    run_app_for(g_lf_config.fork_commit_ms + g_lf_config.fork_reacquire_timeout_ms);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING, "left branch reacquires line and returns RUNNING");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_fork_stale_radar_uses_fallback(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    set_fork_line();
    run_app_for(g_lf_config.fork_sample_ms + 60U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FORK_COMMIT_LEFT,
                            "stale radar uses default left fallback");
    failures += expect_true(ctx->fork_decision == -1, "stale radar records fallback decision");
    failures += expect_true(ctx->fork_radar_stale, "stale radar flag recorded");
    failures += expect_true(ctx->reason == LF_APP_REASON_FORK_FALLBACK_LEFT, "stale radar fallback reason recorded");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_false_fork_rejected_and_obstacle_still_avoids(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    set_left_only_line();
    run_app_for(40U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_RUNNING, "single-side line does not enter fork state");

    inject_block_frames();
    run_app_step_after(10U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_AVOID_PREP,
                            "non-fork radar block still enters generic avoidance");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_fork_out_of_band_target_not_blocked(void)
{
    const LF_AppContext *ctx;
    int failures = 0;

    if (init_app_to_running()) {
        return 1;
    }

    inject_ld2410s_minimal_frame(150U, 2U);
    set_fork_line();
    run_app_for(g_lf_config.fork_sample_ms + 60U);
    ctx = LF_App_GetContext();
    failures += expect_true(ctx->state == LF_APP_STATE_FORK_COMMIT_LEFT,
                            "out-of-band target does not block left fork");
    failures += expect_true(ctx->fork_decision == -1, "out-of-band target keeps left decision");
    failures += expect_true(ctx->reason == LF_APP_REASON_FORK_LEFT_CLEAR, "out-of-band target records clear reason");
    LF_PlatformStub_ClearLineSensorRaw();
    return failures;
}

static int test_app_avoidance(void)
{
    const LF_AppContext *ctx;

    if (init_app_to_running()) {
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
    failures += test_sensor_isolated_edge_noise_ignored_for_position();
    failures += test_pid_output_slew_limit();
    failures += test_app_degraded_calibration_runs_limited();
    failures += test_debug_monitor_raw_and_reason();
    failures += test_app_recovery_timeout();
    failures += test_single_edge_hint_noise_does_not_slow_to_curve_speed();
    failures += test_offset_line_still_turns_with_deadband();
    failures += test_wide_center_deadband_softens_motor_output();
    failures += test_start_straight_guard_limits_wide_black_turn();
    failures += test_edge_realign_uses_small_motor_delta();
    failures += test_curve_arc_uses_independent_small_delta();
    failures += test_side_wide_lamps_enter_curve_arc_before_edge_realign();
    failures += test_curve_arc_preserves_pid_state();
    failures += test_straight_noise_keeps_base_speed();
    failures += test_arrow_like_wide_frame_does_not_start_lead_without_entry();
    failures += test_arrow_interference_drives_straight_on_low_speed_line();
    failures += test_left_right_angle_reorient_accepts_ch8_dark();
    failures += test_left_right_angle_reorient_accepts_ch7_ch8_dark();
    failures += test_left_right_angle_uses_current_sample_not_filtered_residue();
    failures += test_fixed_turn_overrides_existing_right_angle_reorient();
    failures += test_route_all_on_t_turns_right_without_arm_delay();
    failures += test_fixed_turn_right_runs_full_duration_even_when_center_seen();
    failures += test_route_counts_two_all_on_crosses_then_next_left_turn();
    failures += test_biased_wide_noise_does_not_start_lead_event();
    failures += test_lead_event_advances_before_turning();
    failures += test_lead_event_without_entry_direction_does_not_random_turn();
    failures += test_line_loss_resets_lead_phase();
    failures += test_fork_left_blocked_commits_right();
    failures += test_fork_left_clear_commits_left();
    failures += test_fork_stale_radar_uses_fallback();
    failures += test_false_fork_rejected_and_obstacle_still_avoids();
    failures += test_fork_out_of_band_target_not_blocked();
    failures += test_app_avoidance();

    if (failures != 0) {
        printf("test_lf failed: %d\n", failures);
        return 1;
    }

    printf("test_lf passed\n");
    return 0;
}
