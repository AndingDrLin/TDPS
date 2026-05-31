#include "lf_config.h"

/*
 * 配置预设：修改本文件中的 profile 函数来切换测试 / 比赛参数。
 * 在 main.c 中调用一个 profile 函数即可覆盖默认值。
 */

void LF_Config_ApplyDebugProfile(void)
{
    g_lf_config.sensor_input_mode = LF_SENSOR_INPUT_UART_PROTOCOL;
    g_lf_config.sensor_use_dynamic_calibration = false;
    g_lf_config.sensor_fast_calibration = true;
    g_lf_config.sensor_invert_polarity = true;
    g_lf_config.sensor_digital_active_high = false;
    g_lf_config.sensor_filter_alpha = 0.45f;
    g_lf_config.line_detect_min_sum = 780U;
    g_lf_config.line_detect_min_peak = 260U;
    g_lf_config.line_detect_min_contrast = 80U;
    g_lf_config.sensor_weights[0] = 1750;
    g_lf_config.sensor_weights[1] = 1250;
    g_lf_config.sensor_weights[2] = 750;
    g_lf_config.sensor_weights[3] = 250;
    g_lf_config.sensor_weights[4] = -250;
    g_lf_config.sensor_weights[5] = -750;
    g_lf_config.sensor_weights[6] = -1250;
    g_lf_config.sensor_weights[7] = -1750;
    g_lf_config.kp = 0.08f;
    g_lf_config.ki = 0.0f;
    g_lf_config.kd = 0.20f;
    g_lf_config.base_speed = 100;
    g_lf_config.adaptive_slow_speed = 80;
    g_lf_config.sharp_turn_speed = 80;
    g_lf_config.max_correction = 50;
    g_lf_config.derivative_filter_alpha = 0.60f;
    g_lf_config.max_output_delta_per_tick = 15;
    g_lf_config.max_motor_cmd = 300;
    g_lf_config.motor_deadband = 0;
    g_lf_config.auto_start_delay_ms = 0U;
    g_lf_config.start_min_boot_delay_ms = 2000U;
    g_lf_config.start_line_hold_ms = 500U;
    g_lf_config.calibration_duration_ms = 500U;
    g_lf_config.radar_enable = true;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.fork_enable = false;
    g_lf_config.recover_timeout_ms = 3000U;
    g_lf_config.recover_turn_speed = 0;
    g_lf_config.recover_backtrack_speed = 0;
    g_lf_config.line_hold_speed = 0;
    g_lf_config.line_hold_turn_speed = 0;
}

void LF_Config_ApplyTrackProfile(void)
{
    g_lf_config.sensor_input_mode = LF_SENSOR_INPUT_UART_PROTOCOL;
    g_lf_config.sensor_use_dynamic_calibration = false;
    g_lf_config.sensor_fast_calibration = true;
    g_lf_config.sensor_invert_polarity = true;
    g_lf_config.sensor_digital_active_high = false;
    g_lf_config.sensor_filter_alpha = 0.35f;
    g_lf_config.line_detect_min_sum = 780U;
    g_lf_config.line_detect_min_peak = 260U;
    g_lf_config.line_detect_min_contrast = 80U;
    g_lf_config.sensor_weights[0] = 1750;
    g_lf_config.sensor_weights[1] = 1250;
    g_lf_config.sensor_weights[2] = 750;
    g_lf_config.sensor_weights[3] = 250;
    g_lf_config.sensor_weights[4] = -250;
    g_lf_config.sensor_weights[5] = -750;
    g_lf_config.sensor_weights[6] = -1250;
    g_lf_config.sensor_weights[7] = -1750;
    g_lf_config.kp = 0.07f;
    g_lf_config.ki = 0.0f;
    g_lf_config.kd = 0.22f;
    g_lf_config.base_speed = 280;
    g_lf_config.adaptive_slow_speed = 210;
    g_lf_config.sharp_turn_speed = 145;
    g_lf_config.max_correction = 70;
    g_lf_config.straight_boost_enable = true;
    g_lf_config.curve_prepare_enable = true;
    g_lf_config.line_stability_enable = true;
    g_lf_config.stable_direction_enable = true;
    g_lf_config.straight_boost_speed = 340;
    g_lf_config.straight_error_deadband = 150;
    g_lf_config.straight_error_scale_percent = 35U;
    g_lf_config.straight_correction_limit = 20;
    g_lf_config.curve_prepare_speed = 170;
    g_lf_config.derivative_filter_alpha = 0.85f;
    g_lf_config.max_output_delta_per_tick = 8;
    g_lf_config.max_motor_cmd = 800;
    g_lf_config.motor_deadband = 80;
    g_lf_config.auto_start_delay_ms = 0U;
    g_lf_config.start_min_boot_delay_ms = 2000U;
    g_lf_config.start_line_hold_ms = 500U;
    g_lf_config.calibration_duration_ms = 500U;
    g_lf_config.radar_enable = true;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.fork_enable = true;
    g_lf_config.fork_detect_confirm_ticks = 3U;
    g_lf_config.line_lost_grace_ticks = 5U;
    g_lf_config.line_hold_speed = 90;
    g_lf_config.line_hold_turn_speed = 60;
    g_lf_config.recover_backtrack_speed = 80;
    g_lf_config.recover_sweep_start_speed = 100;
    g_lf_config.recover_sweep_max_speed = 190;
    g_lf_config.recover_confirm_ticks = 4U;
    g_lf_config.recover_timeout_ms = 1500U;
}

void LF_Config_ApplyCompetitionProfile(void)
{
    g_lf_config.base_speed = 400;
    g_lf_config.adaptive_slow_speed = 280;
    g_lf_config.max_correction = 400;
    g_lf_config.max_motor_cmd = 900;
    g_lf_config.auto_start_delay_ms = 800U;
    g_lf_config.sensor_fast_calibration = false;
    g_lf_config.fork_enable = true;
    g_lf_config.recover_timeout_ms = 1100U;
}
