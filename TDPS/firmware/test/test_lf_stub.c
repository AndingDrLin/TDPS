/**
 * @file test_lf_stub.c
 * @brief Firmware PC stub regression tests for line-following core.
 *
 * The updated firmware removed patch-style states; the tests only cover
 * the retained general core: sensors, radar, PD control, startup
 * calibration, line-loss recovery, and fixed-route scripted turns.
 */
#include "lf_app.h"
#include "lf_config.h"
#include "lf_control.h"
#include "lf_debug_monitor.h"
#include "lf_platform.h"
#include "lf_radar.h"
#include "lf_sensor.h"

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

static void set_raw_all(uint16_t value)
{
    uint16_t raw[LF_SENSOR_COUNT];
    uint8_t i;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        raw[i] = value;
    }
    LF_PlatformStub_SetLineSensorRaw(raw);
}

static void set_digital_line(uint8_t mask)
{
    uint16_t raw[LF_SENSOR_COUNT];
    uint8_t digital[LF_SENSOR_COUNT];
    uint8_t i;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        if ((mask & (uint8_t)(1U << i)) != 0U) {
            raw[i] = 4095U;
            digital[i] = 0U;   /* D-frame: 0 = black line = LED on */
        } else {
            raw[i] = 0U;
            digital[i] = 1U;
        }
    }

    LF_PlatformStub_SetLineSensorRaw(raw);
    LF_PlatformStub_SetDigitalFrame(digital);
}

static void step_ms(uint32_t ms)
{
    LF_Platform_DelayMs(ms);
    LF_App_RunStep();
}

static void run_app_for(uint32_t ms)
{
    uint32_t elapsed = 0U;
    while (elapsed < ms) {
        step_ms(g_lf_config.control_period_ms);
        elapsed += g_lf_config.control_period_ms;
    }
}

static void setup_basic_app(void)
{
    LF_Platform_BoardInit();
    LF_Config_ApplyDebugProfile();

    g_lf_config.sensor_input_mode = LF_SENSOR_INPUT_DIGITAL_GPIO;
    g_lf_config.sensor_digital_active_high = true;
    g_lf_config.sensor_digital_threshold = 2000U;
    g_lf_config.sensor_invert_polarity = false;
    g_lf_config.sensor_use_dynamic_calibration = false;
    g_lf_config.sensor_fast_calibration = true;
    g_lf_config.sensor_filter_alpha = 1.0f;
    g_lf_config.auto_start_delay_ms = 10U;
    g_lf_config.start_min_boot_delay_ms = 0U;
    g_lf_config.start_line_hold_ms = 0U;
    g_lf_config.calibration_duration_ms = 0U;
    g_lf_config.radar_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.fixed_turn_enable = false;
    g_lf_config.route_script_enable = false;
    g_lf_config.reorient_enable = false;
    g_lf_config.segment_control_enable = false;
    g_lf_config.max_motor_delta = 0;
    g_lf_config.motor_deadband = 0;

    LF_DebugMonitor_Init();
    set_digital_line(0x18U);  /* middle two channels */
    LF_App_Init();
    run_app_for(40U);
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
           expect_true(radar->distance_mm == 100U, "minimal frame converts cm to mm");
}

static int test_sensor_weighted_position(void)
{
    LF_SensorFrame frame;

    LF_Platform_BoardInit();
    g_lf_config.sensor_input_mode = LF_SENSOR_INPUT_DIGITAL_GPIO;
    g_lf_config.sensor_digital_active_high = true;
    g_lf_config.sensor_digital_threshold = 2000U;
    g_lf_config.sensor_filter_alpha = 1.0f;
    g_lf_config.line_detect_min_sum = 1000U;
    g_lf_config.line_detect_min_peak = 500U;
    g_lf_config.line_detect_min_contrast = 0U;
    g_lf_config.sensor_weights[0] = -3500;
    g_lf_config.sensor_weights[1] = -2500;
    g_lf_config.sensor_weights[2] = -1500;
    g_lf_config.sensor_weights[3] = -500;
    g_lf_config.sensor_weights[4] = 500;
    g_lf_config.sensor_weights[5] = 1500;
    g_lf_config.sensor_weights[6] = 2500;
    g_lf_config.sensor_weights[7] = 3500;

    LF_Sensor_Init();
    LF_Sensor_StartCalibration();
    set_digital_line(0x30U);  /* ch4, ch5 */
    LF_Sensor_ReadFrame(&frame);

    return expect_true(frame.line_detected, "sensor detects digital line") |
           expect_true(frame.position == 1000, "weighted position equals center of ch4/ch5") |
           expect_true(frame.edge_hint == 1, "edge hint points right");
}

static int test_control_pd_turns_toward_error(void)
{
    LF_PIDState pid;
    int16_t corr;
    int16_t left;
    int16_t right;

    LF_Control_ResetPid(&pid);
    g_lf_config.kp = 0.2f;
    g_lf_config.kd = 0.0f;
    g_lf_config.kff = 0.0f;
    g_lf_config.max_correction = 300;
    g_lf_config.max_motor_cmd = 900;
    g_lf_config.steering_dir_sign = +1;
    g_lf_config.derivative_filter_alpha = 0.0f;
    g_lf_config.max_output_delta_per_tick = 0;

    corr = LF_Control_UpdatePD(500.0f, 0.01f, 100, &pid);
    LF_Control_ComputeMotorCmd(100, corr, &left, &right);

    return expect_true(corr == 100, "PD P-only correction") |
           expect_true(left == 200 && right == 0, "positive error accelerates left wheel");
}

static int test_app_starts_and_runs(void)
{
    const LF_AppContext *ctx;
    int16_t left;
    int16_t right;

    setup_basic_app();
    ctx = LF_App_GetContext();
    LF_DebugMonitor_GetLastMotorCommand(&left, &right);

    return expect_true(ctx->state == LF_APP_STATE_RUNNING, "app reaches RUNNING") |
           expect_true(ctx->calibration_ok, "fast calibration succeeds") |
           expect_true(left != 0 || right != 0, "running outputs motor command");
}

static int test_line_loss_enters_recovery(void)
{
    const LF_AppContext *ctx;

    setup_basic_app();
    set_digital_line(0x00U);
    run_app_for((uint32_t)(g_lf_config.line_lost_grace_ticks + 2U) * g_lf_config.control_period_ms);
    ctx = LF_App_GetContext();

    return expect_true(ctx->state == LF_APP_STATE_RECOVERING, "line loss enters recovery");
}

static int test_route_first_t_triggers_fixed_turn(void)
{
    const LF_AppContext *ctx;

    setup_basic_app();
    g_lf_config.route_script_enable = true;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_stop_ms = 0U;
    g_lf_config.fixed_turn_90_ms_left = 80U;
    g_lf_config.fixed_turn_90_ms_right = 80U;
    g_lf_config.fixed_turn_settle_ms = 20U;
    g_lf_config.fixed_turn_cooldown_ms = 0U;

    set_digital_line(0xFFU);  /* T-junction / all channels lit */
    run_app_for(20U);
    ctx = LF_App_GetContext();

    return expect_true(ctx->state == LF_APP_STATE_FIXED_TURN_SPIN ||
                       ctx->state == LF_APP_STATE_FIXED_TURN_SETTLE,
                       "first all-on route event starts fixed turn") |
           expect_true(ctx->fixed_turn_dir == +1, "first T route turns right");
}

static int test_fixed_turn_returns_to_running(void)
{
    const LF_AppContext *ctx;

    setup_basic_app();
    g_lf_config.route_script_enable = true;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_stop_ms = 0U;
    g_lf_config.fixed_turn_90_ms_left = 30U;
    g_lf_config.fixed_turn_90_ms_right = 30U;
    g_lf_config.fixed_turn_settle_ms = 10U;
    g_lf_config.fixed_turn_cooldown_ms = 0U;

    set_digital_line(0xFFU);
    run_app_for(20U);
    set_digital_line(0x18U);
    run_app_for(80U);
    ctx = LF_App_GetContext();

    return expect_true(ctx->state == LF_APP_STATE_RUNNING, "fixed turn completes and returns to RUNNING") |
           expect_true(ctx->route_phase == (uint8_t)LF_ROUTE_PHASE_LEFT_1, "route phase committed after first T");
}

int main(void)
{
    int failures = 0;

    failures += test_radar_simple_block();
    failures += test_radar_minimal_clear_state();
    failures += test_sensor_weighted_position();
    failures += test_control_pd_turns_toward_error();
    failures += test_app_starts_and_runs();
    failures += test_line_loss_enters_recovery();
    failures += test_route_first_t_triggers_fixed_turn();
    failures += test_fixed_turn_returns_to_running();

    if (failures == 0) {
        printf("All LF stub tests passed.\n");
    } else {
        printf("LF stub tests failed: %d\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
