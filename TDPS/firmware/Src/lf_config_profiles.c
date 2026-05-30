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
    g_lf_config.sensor_invert_polarity = false;
    g_lf_config.sensor_digital_active_high = false;
    g_lf_config.sensor_filter_alpha = 0.60f;
    g_lf_config.line_detect_min_sum = 780U;
    g_lf_config.line_detect_min_peak = 260U;
    g_lf_config.line_detect_min_contrast = 80U;
    g_lf_config.base_speed = 180;
    g_lf_config.adaptive_slow_speed = 120;
    g_lf_config.max_correction = 250;
    g_lf_config.max_motor_cmd = 600;
    g_lf_config.motor_deadband = 0;
    g_lf_config.auto_start_delay_ms = 0U;
    g_lf_config.start_min_boot_delay_ms = 2000U;
    g_lf_config.start_line_hold_ms = 2000U;
    g_lf_config.calibration_duration_ms = 500U;
    g_lf_config.radar_enable = true;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.fork_enable = false;
    g_lf_config.recover_timeout_ms = 3000U;
    g_lf_config.recover_turn_speed = 150;
    g_lf_config.recover_backtrack_speed = 100;
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
