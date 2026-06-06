/**
 * @file lf_config_profiles.c
 * @brief 实车调试配置预设
 */
#include "lf_config.h"

/**
 * @brief 低速保守调试配置
 *
 * 当前实车路径需要保留：路线脚本、固定 90° 转弯、分段控制。
 * 已删除的补丁式逻辑（前探补偿、边缘对齐、弯道弧线、T口后退等）不再配置。
 */
void LF_Config_ApplyDebugProfile(void)
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
    g_lf_config.sensor_edge_noise_reject_enable = true;
    g_lf_config.sensor_edge_noise_neighbor_threshold = 220U;

    /* 倒三轮实车的传感器方向：position 左正右负，配合 steering_dir_sign=-1。 */
    g_lf_config.sensor_weights[0] =  1950;
    g_lf_config.sensor_weights[1] =  1450;
    g_lf_config.sensor_weights[2] =   950;
    g_lf_config.sensor_weights[3] =   250;
    g_lf_config.sensor_weights[4] =  -250;
    g_lf_config.sensor_weights[5] =  -750;
    g_lf_config.sensor_weights[6] = -1450;
    g_lf_config.sensor_weights[7] = -1950;

    /* 简化 PD + kff：先稳住直线，再逐步加前馈。 */
    g_lf_config.kp = 0.25f;
    g_lf_config.kd = 0.60f;
    g_lf_config.kff = 0.0f;
    g_lf_config.base_speed = 100;
    g_lf_config.min_speed = 60;
    g_lf_config.max_correction = 300;
    g_lf_config.steering_dir_sign = -1;
    g_lf_config.derivative_filter_alpha = 0.35f;
    g_lf_config.max_output_delta_per_tick = 15;
    g_lf_config.max_motor_cmd = 900;
    g_lf_config.max_motor_delta = 800;
    g_lf_config.motor_deadband = 80;

    g_lf_config.auto_start_delay_ms = 0U;
    g_lf_config.start_min_boot_delay_ms = 2000U;
    g_lf_config.start_line_hold_ms = 500U;
    g_lf_config.calibration_duration_ms = 500U;

    /* 丢线恢复：短前冲、短后退、按最后可信方向扫描。 */
    g_lf_config.line_lost_grace_ticks = 4U;
    g_lf_config.recover_forward_speed = 100;
    g_lf_config.recover_backtrack_speed = 80;
    g_lf_config.recover_forward_ms = 80U;
    g_lf_config.recover_backtrack_ms = 120U;
    g_lf_config.recover_sweep_speed = 120;
    g_lf_config.recover_sweep_ms = 420U;
    g_lf_config.recover_timeout_ms = 3000U;

    /* 直角原地旋转：作为非脚本直角的兜底。 */
    g_lf_config.reorient_enable = true;
    g_lf_config.reorient_spin_speed = 180;
    g_lf_config.reorient_timeout_ms = 5000U;
    g_lf_config.reorient_position_threshold = 520;
    g_lf_config.reorient_cooldown_ms = 1500U;
    g_lf_config.reorient_confirm_ticks = 1U;

    /* 路线脚本 + 固定 90° 转弯：保留硬赛道通过能力。 */
    g_lf_config.route_script_enable = true;
    g_lf_config.route_event_confirm_ticks = 1U;
    g_lf_config.route_event_cooldown_ms = 0U;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_spin_speed = 400;
    g_lf_config.fixed_turn_stop_ms = 60U;
    g_lf_config.fixed_turn_90_ms_left = 3500U;
    g_lf_config.fixed_turn_90_ms_right = 3500U;
    g_lf_config.fixed_turn_settle_ms = 80U;
    g_lf_config.fixed_turn_cooldown_ms = 600U;

    /* 分段控制：只保留直道 / 弯道两组参数，减少调参维度。 */
    g_lf_config.segment_control_enable = true;
    g_lf_config.segment_confirm_ticks = 1U;
    g_lf_config.segment_hold_ticks = 3U;
    g_lf_config.seg_kp_straight = 0.16f;
    g_lf_config.seg_kd_straight = 1.25f;
    g_lf_config.seg_kff_straight = 0.0f;
    g_lf_config.seg_base_speed_straight = 100;
    g_lf_config.seg_min_speed_straight = 80;
    g_lf_config.seg_max_correction_straight = 180;
    g_lf_config.seg_kp_curve = 0.70f;
    g_lf_config.seg_kd_curve = 0.50f;
    g_lf_config.seg_kff_curve = 0.0007f;
    g_lf_config.seg_base_speed_curve = 65;
    g_lf_config.seg_min_speed_curve = 35;
    g_lf_config.seg_max_correction_curve = 700;

    /* 目前硬路线调试优先，不启用雷达避障。 */
    g_lf_config.radar_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
}
