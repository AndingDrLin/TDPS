#include "lf_config.h"

#ifndef TDPS_SIM_CALIBRATION_DURATION_MS
#define TDPS_SIM_CALIBRATION_DURATION_MS 3000U
#endif

#ifndef TDPS_SIM_CALIBRATION_SWITCH_MS
#define TDPS_SIM_CALIBRATION_SWITCH_MS 300U
#endif

#ifndef TDPS_SIM_CALIBRATION_SPIN_SPEED
#define TDPS_SIM_CALIBRATION_SPIN_SPEED 180
#endif

#ifndef TDPS_SIM_SENSOR_ALPHA
#define TDPS_SIM_SENSOR_ALPHA 0.35f
#endif

#ifndef TDPS_SIM_LINE_MIN_SUM
#define TDPS_SIM_LINE_MIN_SUM 780U
#endif

#ifndef TDPS_SIM_EDGE_HINT
#define TDPS_SIM_EDGE_HINT 120U
#endif

#ifndef TDPS_SIM_KP
#define TDPS_SIM_KP 0.48f
#endif

#ifndef TDPS_SIM_KI
#define TDPS_SIM_KI 0.0f
#endif

#ifndef TDPS_SIM_KD
#define TDPS_SIM_KD 1.90f
#endif

#ifndef TDPS_SIM_BASE_SPEED
#define TDPS_SIM_BASE_SPEED 320
#endif

#ifndef TDPS_SIM_MAX_CORRECTION
#define TDPS_SIM_MAX_CORRECTION 340
#endif

#ifndef TDPS_SIM_RECOVER_TURN
#define TDPS_SIM_RECOVER_TURN 220
#endif

#ifndef TDPS_SIM_RECOVER_TIMEOUT
#define TDPS_SIM_RECOVER_TIMEOUT 900U
#endif

#ifndef TDPS_SIM_DYNAMIC_CALIBRATION
#define TDPS_SIM_DYNAMIC_CALIBRATION true
#endif

LF_Config g_lf_config = {
    .control_period_ms = 10U,
    .auto_start_delay_ms = 1200U,
    .calibration_duration_ms = TDPS_SIM_CALIBRATION_DURATION_MS,
    .calibration_switch_interval_ms = TDPS_SIM_CALIBRATION_SWITCH_MS,
    .calibration_spin_speed = TDPS_SIM_CALIBRATION_SPIN_SPEED,
    .sensor_filter_alpha = TDPS_SIM_SENSOR_ALPHA,
    .sensor_input_mode = LF_SENSOR_INPUT_ANALOG_ADC,
    .sensor_invert_polarity = false,
    .sensor_digital_threshold = 2048U,
    .sensor_digital_active_high = false,
    .sensor_use_dynamic_calibration = TDPS_SIM_DYNAMIC_CALIBRATION,
    .sensor_allow_degraded_calibration = true,
    .sensor_min_valid_count = 5U,
    .sensor_calibration_min_delta = 20U,
    .sensor_degraded_max_speed = 180,
    .line_detect_min_sum = TDPS_SIM_LINE_MIN_SUM,
    .line_detect_min_peak = 260U,
    .line_detect_min_contrast = 120U,
    .line_lost_grace_ticks = 4U,
    .edge_hint_threshold = TDPS_SIM_EDGE_HINT,
    .sensor_weights = {-1750, -1250, -750, -250, 250, 750, 1250, 1750},
    .kp = TDPS_SIM_KP,
    .ki = TDPS_SIM_KI,
    .kd = TDPS_SIM_KD,
    .base_speed = 300,
    .max_correction = TDPS_SIM_MAX_CORRECTION,
    .adaptive_slow_speed = 210,
    .adaptive_error_threshold = 800,
    .adaptive_confidence_threshold = 0.40f,
    .sharp_turn_speed = 190,
    .straight_boost_enable = true,
    .curve_prepare_enable = true,
    .line_stability_enable = true,
    .stable_direction_enable = true,
    .straight_boost_speed = 320,
    .straight_error_threshold = 250,
    .straight_delta_threshold = 120,
    .straight_confidence_min = 0.55f,
    .straight_confirm_ticks = 8U,
    .curve_prepare_speed = 150,
    .curve_prepare_error_threshold = 600,
    .curve_prepare_delta_threshold = 180,
    .curve_prepare_confirm_ticks = 2U,
    .interference_active_count_threshold = 6U,
    .interference_position_jump_threshold = 450,
    .direction_update_confidence_min = 0.45f,
    .line_hold_speed = 150,
    .line_hold_turn_speed = 210,
    .derivative_filter_alpha = 0.75f,
    .integral_limit = 5000.0f,
    .max_output_delta_per_tick = 80,
    .max_motor_cmd = 900,
    .motor_deadband = 120,
    .recover_turn_speed = TDPS_SIM_RECOVER_TURN,
    .recover_backtrack_speed = 140,
    .recover_backtrack_ms = 120U,
    .recover_sweep_start_speed = 180,
    .recover_sweep_max_speed = 280,
    .recover_confirm_ticks = 3U,
    .recover_confidence_min = 0.25f,
    .recover_timeout_ms = TDPS_SIM_RECOVER_TIMEOUT,
    .radar_enable = true,
    .radar_uart_baudrate = 256000U,
    .radar_trigger_distance_mm = 450U,
    .radar_release_distance_mm = 650U,
    .radar_debounce_frames = 3U,
    .radar_frame_timeout_ms = 300U,
    .obstacle_warn_speed = 220,
    .obstacle_avoid_enable = true,
    .obstacle_preferred_side = 0,
    .obstacle_max_attempts = 2U,
    .obstacle_confirm_ms = 100U,
    .obstacle_stop_ms = 120U,
    .obstacle_turn_out_ms = 360U,
    .obstacle_bypass_ms = 900U,
    .obstacle_turn_in_ms = 360U,
    .obstacle_reacquire_timeout_ms = 1600U,
    .obstacle_turn_speed = 240,
    .obstacle_bypass_inner_speed = 160,
    .obstacle_bypass_outer_speed = 320,
    .obstacle_emergency_distance_mm = 280U,
    .fork_enable = false,
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
    .fork_radar_block_confirm_frames = 2U,
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
