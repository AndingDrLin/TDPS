#include "lf_config.h"

/*
 * 整车测试只改本文件中的 g_lf_config。
 * 推荐顺序：传感器极性/阈值 -> base_speed -> kp/kd -> 雷达阈值 -> 避障动作时间。
 */

LF_Config g_lf_config = {
    .control_period_ms = 10U,
    .auto_start_delay_ms = 1200U,
    .start_min_boot_delay_ms = 2000U,
    .start_line_hold_ms = 2000U,

    /* 标定参数：影响上电后传感器 min/max 覆盖质量。 */
    .calibration_duration_ms = 3000U,
    .calibration_switch_interval_ms = 300U,
    .calibration_spin_speed = 180,

    /* 传感器参数：先确认黑线/白底读数方向，再调 line_detect_min_sum。 */
    .sensor_filter_alpha = 0.35f,
    .sensor_input_mode = LF_SENSOR_INPUT_UART_PROTOCOL,
    .sensor_invert_polarity = false,
    .sensor_digital_threshold = 2048U,
    .sensor_digital_active_high = false,
    .sensor_use_dynamic_calibration = true,
    .sensor_allow_degraded_calibration = true,
    .sensor_min_valid_count = 5U,
    .sensor_calibration_min_delta = 20U,
    .sensor_degraded_max_speed = 180,
    .line_detect_min_sum = 780U,
    .line_detect_min_peak = 260U,
    .line_detect_min_contrast = 120U,
    .line_lost_grace_ticks = 4U,
    .edge_hint_threshold = 120U,
    .sensor_edge_noise_reject_enable = true,
    .sensor_edge_noise_neighbor_threshold = 260U,
    .sensor_weights = {-3200, -1800, -800, -200, 200, 800, 1800, 3200},

    /*
     * 巡线 PID：5 级非线性响应链 ——
     * deadband(50) → soft_zone(150) → 线性区 → 积分过渡区 → 积分分离区。
     * Ki 只在 |error| < sep 时全速累积，超过 sep+soft 则指数衰减。
     */
    .kp = 0.10f,       /* P 项：保守值，兼顾直线稳定；有 deadband/变化率限幅保护后可调高 */
    .ki = 0.0f,        /* 简化 PD——巡线无静差，积分在弯道累积导致过冲 */
    .kd = 1.20f,       /* D 项：需配合 derivative_filter_alpha 抑制 dt=0.01 噪声放大 */
    .base_speed = 180, /* 实车直线速度：模拟器 280 在实车上摇头，先降速稳直线 */
    .min_speed = 60,   /* 弯道最低速 */
    .kff = 0.0f,       /* 先关 kff 测纯 PD；直线稳后再从小到大加（0.0003→0.0005→0.0008） */
    .max_correction = 300, /* 差速上限：300 明显优于 180 */
    .control_error_deadband = 0,
    .control_error_soft_zone = 0,
    .adaptive_slow_speed = 210,
    .adaptive_error_threshold = 350,
    .adaptive_confidence_threshold = 0.40f,
    .sharp_turn_speed = 190,
    .straight_boost_enable = false,
    .curve_prepare_enable = false,
    .line_stability_enable = true,
    .stable_direction_enable = true,
    .straight_boost_speed = 320,
    .straight_error_threshold = 250,
    .straight_delta_threshold = 120,
    .straight_confidence_min = 0.55f,
    .straight_confirm_ticks = 8U,
    .curve_prepare_speed = 35,
    .curve_prepare_error_threshold = 250,
    .curve_prepare_delta_threshold = 70,
    .curve_prepare_confirm_ticks = 1U,
    .interference_active_count_threshold = 6U,
    .interference_position_jump_threshold = 450,
    .direction_update_confidence_min = 0.45f,
    .lead_compensation_enable = true,
    .lead_event_active_count_threshold = 6U,
    .lead_event_min_sum = 2200U,
    .lead_event_center_error_threshold = 350,
    .lead_event_entry_error_threshold = 650,
    .lead_event_confirm_ticks = 2U,
    .lead_advance_ticks = 18U,
    .lead_advance_speed = 90,
    .lead_turn_hold_ticks = 14U,
    .lead_turn_speed = 70,
    .lead_turn_delta = 140,
    .lead_entry_memory_ticks = 35U,
    .straight_noise_reject_enable = true,
    .straight_noise_confirm_ticks = 2U,
    .straight_noise_active_count_threshold = 5U,
    .straight_noise_max_sum = 1800U,
    .straight_noise_max_position_error = 250,
    .straight_noise_max_position_delta = 120,
    .line_hold_speed = 150,
    .line_hold_turn_speed = 210,
    .derivative_filter_alpha = 0.0f,  /* D 项一阶低通滤波：UpdatePid 和 UpdatePD 共用，实车建议 0.35 */
    /* 积分保护链：硬限幅 → 分离衰减 → 输出变化率限幅 → 反饱和回退 */
    .integral_limit = 0.0f,
    .integral_separation_threshold = 0.0f,
    .integral_soft_zone = 0.0f,
    .max_output_delta_per_tick = 0,  /* 单拍修正变化上限：UpdatePid 和 UpdatePD 共用，实车建议 30 */
    .max_motor_cmd = 900,
    .motor_deadband = 120,

    /* 丢线恢复：先短退再扩大搜索，确认稳定回线后恢复 RUNNING。 */
    .recover_turn_speed = 220,
    .recover_backtrack_speed = 140,
    .recover_backtrack_ms = 120U,
    .recover_sweep_start_speed = 180,
    .recover_sweep_max_speed = 280,
    .recover_confirm_ticks = 3U,
    .recover_confidence_min = 0.25f,
    .recover_timeout_ms = 1100U,

    /* 雷达判定：距离单位统一为 mm，实机需确认 LD2410S 波特率和距离单位。 */
    .radar_enable = true,
    .radar_uart_baudrate = 115200U,
    .radar_trigger_distance_mm = 450U,
    .radar_release_distance_mm = 650U,
    .radar_debounce_frames = 3U,
    .radar_frame_timeout_ms = 300U,

    /* 避障动作：定时开环绕障，上板主要调 turn/bypass/reacquire 时间和速度。 */
    .obstacle_warn_speed = 220,
    .obstacle_avoid_enable = true,
    .obstacle_preferred_side = 0,
    .obstacle_max_attempts = 2U,
    .obstacle_confirm_ms = 100U,
    .obstacle_stop_ms = 120U,
    .obstacle_turn_out_ms = 360U,
    .obstacle_turn_out_min_ms = 160U,
    .obstacle_bypass_ms = 900U,
    .obstacle_bypass_min_ms = 320U,
    .obstacle_turn_in_ms = 360U,
    .obstacle_turn_in_min_ms = 160U,
    .obstacle_reacquire_timeout_ms = 1600U,
    .obstacle_safe_distance_mm = 700U,
    .obstacle_line_reacquire_min_sum = 900U,
    .obstacle_reacquire_confirm_ticks = 3U,
    .obstacle_turn_speed = 240,
    .obstacle_bypass_inner_speed = 160,
    .obstacle_bypass_outer_speed = 320,
    .obstacle_emergency_distance_mm = 280U,

    /* 岔路决策：左斜装雷达检测左支路，左堵走右，未确认左堵走左。 */
    .fork_enable = true,
    .fork_detect_min_sum = 1800U,
    .fork_sensor_active_threshold = 450U,
    .fork_detect_min_active_sensors = 5U,
    .fork_left_min_sum = 650U,
    .fork_right_min_sum = 650U,
    .fork_detect_max_abs_position = 650,
    .fork_detect_confirm_ticks = 2U,
    .fork_sample_ms = 800U,
    .fork_radar_max_age_ms = 400U,
    .fork_radar_min_distance_mm = 150U,
    .fork_radar_block_distance_mm = 1000U,
    .fork_radar_block_confirm_frames = 1U,
    .fork_radar_valid_min_samples = 1U,
    .fork_fallback_branch = -1,
    .fork_sample_speed = 0,
    .fork_commit_speed = 210,
    .fork_commit_turn_speed = 170,
    .fork_commit_min_ms = 180U,
    .fork_commit_ms = 520U,
    .fork_reacquire_timeout_ms = 1200U,
    .fork_line_reacquire_min_sum = 820U,
    .fork_reacquire_confirm_ticks = 3U,
    .fork_cooldown_ms = 1200U,

    .sensor_fast_calibration = false,
};
