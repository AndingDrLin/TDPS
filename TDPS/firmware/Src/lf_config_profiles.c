/**
 * @file lf_config_profiles.c
 * @brief Real-car debug configuration presets.
 */
#include "lf_config.h"

/**
 * @brief Low-speed conservative debug profile.
 *
 * The current real-car path retains: route script, fixed 90-degree turns,
 * and segment control.
 * Removed patch-style logic (lead compensation, edge alignment, curve
 * arcs, T-junction back-off, etc.) is no longer configured.
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

    /* Reverse-tricycle sensor direction: position positive-left, negative-right,
     * paired with steering_dir_sign=-1. */
    g_lf_config.sensor_weights[0] =  1950;
    g_lf_config.sensor_weights[1] =  1450;
    g_lf_config.sensor_weights[2] =   950;
    g_lf_config.sensor_weights[3] =   250;
    g_lf_config.sensor_weights[4] =  -250;
    g_lf_config.sensor_weights[5] =  -750;
    g_lf_config.sensor_weights[6] = -1450;
    g_lf_config.sensor_weights[7] = -1950;

    /* Simplified PD + kff: stabilize straight lines first, then gradually add
     * feedforward. */
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

    /* Line-lost recovery: short forward burst, short back-off, sweep in the
     * last credible direction. */
    g_lf_config.line_lost_grace_ticks = 4U;
    g_lf_config.recover_forward_speed = 100;
    g_lf_config.recover_backtrack_speed = 80;
    g_lf_config.recover_forward_ms = 80U;
    g_lf_config.recover_backtrack_ms = 120U;
    g_lf_config.recover_sweep_speed = 120;
    g_lf_config.recover_sweep_ms = 420U;
    g_lf_config.recover_timeout_ms = 3000U;

    /* Right-angle in-place spin: fallback for non-scripted right-angle turns. */
    g_lf_config.reorient_enable = true;
    g_lf_config.reorient_spin_speed = 180;
    g_lf_config.reorient_timeout_ms = 5000U;
    g_lf_config.reorient_position_threshold = 520;
    g_lf_config.reorient_cooldown_ms = 1500U;
    g_lf_config.reorient_confirm_ticks = 1U;

    /* Route script + fixed 90-degree turns: retain hard-track pass capability. */
    g_lf_config.route_script_enable = true;
    g_lf_config.route_event_confirm_ticks = 1U;
    g_lf_config.route_event_cooldown_ms = 0U;
    g_lf_config.fixed_turn_enable = true;
    g_lf_config.fixed_turn_spin_speed = 400;
    g_lf_config.fixed_turn_stop_ms = 60U;
    g_lf_config.fixed_turn_90_ms_left = 800U;
    g_lf_config.fixed_turn_90_ms_right = 800U;
    g_lf_config.fixed_turn_settle_ms = 80U;
    g_lf_config.fixed_turn_cooldown_ms = 600U;

    /* Segment control: only straight / curve parameter sets to reduce tuning
     * dimensions. */
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

    /* Ultrasonic overhead obstacle detection (enabled with 300mm threshold). */
    g_lf_config.ultrasonic_enable = true;
    g_lf_config.ultrasonic_threshold_mm = 300U;
    g_lf_config.ultrasonic_hold_ms = 3000U;
    g_lf_config.ultrasonic_lora_checkpoint_id = 99U;
    g_lf_config.ultrasonic_measure_interval_ms = 80U;
    g_lf_config.ultrasonic_cooldown_ms = 5000U;

    /* Hard-route debugging takes priority; radar avoidance is disabled for now. */
    g_lf_config.radar_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
}
