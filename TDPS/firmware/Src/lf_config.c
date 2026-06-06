/**
 * @file lf_config.c
 * @brief 巡线参数默认值
 */
#include "lf_config.h"

/*
 * 默认值偏保守，适合 PC stub 和首次上板。
 * main.c 会调用 LF_Config_ApplyDebugProfile() 覆盖为当前实车调试参数。
 */
LF_Config g_lf_config = {
    .control_period_ms = 10U,
    .auto_start_delay_ms = 1200U,
    .start_min_boot_delay_ms = 2000U,
    .start_line_hold_ms = 2000U,
    .calibration_duration_ms = 3000U,
    .calibration_switch_interval_ms = 300U,
    .calibration_spin_speed = 180,
    .sensor_fast_calibration = false,

    .sensor_input_mode = LF_SENSOR_INPUT_UART_PROTOCOL,
    .sensor_invert_polarity = false,
    .sensor_digital_threshold = 2048U,
    .sensor_digital_active_high = false,
    .sensor_use_dynamic_calibration = true,
    .sensor_allow_degraded_calibration = true,
    .sensor_min_valid_count = 5U,
    .sensor_calibration_min_delta = 20U,
    .sensor_degraded_max_speed = 180,
    .sensor_filter_alpha = 0.35f,
    .sensor_edge_noise_reject_enable = true,
    .sensor_edge_noise_neighbor_threshold = 260U,
    .sensor_weights = {-3200, -1800, -800, -200, 200, 800, 1800, 3200},

    .line_detect_min_sum = 780U,
    .line_detect_min_peak = 260U,
    .line_detect_min_contrast = 120U,
    .line_lost_grace_ticks = 4U,
    .edge_hint_threshold = 120U,

    .kp = 0.10f,
    .kd = 1.20f,
    .kff = 0.0f,
    .base_speed = 180,
    .min_speed = 60,
    .max_correction = 300,
    .steering_dir_sign = +1,
    .derivative_filter_alpha = 0.0f,
    .max_output_delta_per_tick = 0,
    .max_motor_cmd = 900,
    .max_motor_delta = 600,
    .motor_deadband = 120,

    .recover_forward_speed = 150,
    .recover_backtrack_speed = 140,
    .recover_forward_ms = 100U,
    .recover_backtrack_ms = 120U,
    .recover_sweep_speed = 220,
    .recover_sweep_ms = 400U,
    .recover_timeout_ms = 1100U,

    .radar_enable = true,
    .radar_uart_baudrate = 115200U,
    .radar_trigger_distance_mm = 450U,
    .radar_release_distance_mm = 650U,
    .radar_debounce_frames = 3U,
    .radar_frame_timeout_ms = 300U,
    .obstacle_avoid_enable = true,
    .obstacle_preferred_side = 0,
    .obstacle_stop_ms = 120U,
    .obstacle_bypass_ms = 900U,
    .obstacle_reacquire_timeout_ms = 1600U,
    .obstacle_turn_speed = 240,
    .obstacle_bypass_speed = 240,

    .reorient_enable = false,
    .reorient_spin_speed = 180,
    .reorient_timeout_ms = 3000U,
    .reorient_position_threshold = 900,
    .reorient_cooldown_ms = 1500U,
    .reorient_confirm_ticks = 4U,

    .route_script_enable = false,
    .route_event_confirm_ticks = 1U,
    .route_event_cooldown_ms = 600U,

    .fixed_turn_enable = false,
    .fixed_turn_spin_speed = 200,
    .fixed_turn_stop_ms = 0U,
    .fixed_turn_90_ms_left = 720U,
    .fixed_turn_90_ms_right = 720U,
    .fixed_turn_settle_ms = 80U,
    .fixed_turn_cooldown_ms = 600U,

    .segment_control_enable = false,
    .segment_confirm_ticks = 3U,
    .segment_hold_ticks = 8U,

    .seg_kp_straight = 0.20f,
    .seg_kd_straight = 1.20f,
    .seg_kff_straight = 0.0f,
    .seg_base_speed_straight = 100,
    .seg_min_speed_straight = 80,
    .seg_max_correction_straight = 250,

    .seg_kp_curve = 0.30f,
    .seg_kd_curve = 0.80f,
    .seg_kff_curve = 0.0005f,
    .seg_base_speed_curve = 120,
    .seg_min_speed_curve = 50,
    .seg_max_correction_curve = 400,
};
