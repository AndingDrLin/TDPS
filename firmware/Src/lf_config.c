#include "lf_config.h"

/*
 * 整车测试只改本文件中的 g_lf_config。
 * 推荐顺序：传感器极性/阈值 -> base_speed -> kp/kd -> 雷达阈值 -> 避障动作时间。
 */

const LF_Config g_lf_config = {
    .control_period_ms = 10U,
    .auto_start_delay_ms = 1200U,

    /* 标定参数：影响上电后传感器 min/max 覆盖质量。 */
    .calibration_duration_ms = 3000U,
    .calibration_switch_interval_ms = 300U,
    .calibration_spin_speed = 180,

    /* 传感器参数：先确认黑线/白底读数方向，再调 line_detect_min_sum。 */
    .sensor_filter_alpha = 0.35f,
    .sensor_input_mode = LF_SENSOR_INPUT_ANALOG_ADC,
    .sensor_invert_polarity = false,
    .sensor_digital_threshold = 2048U,
    .sensor_digital_active_high = false,
    .sensor_use_dynamic_calibration = true,
    .line_detect_min_sum = 780U,
    .edge_hint_threshold = 120U,
    .sensor_weights = {-1750, -1250, -750, -250, 250, 750, 1250, 1750},

    /* 巡线 PID：整车速度和稳定性主要由这组参数决定。 */
    .kp = 0.48f,
    .ki = 0.0f,
    .kd = 1.90f,
    .base_speed = 320,
    .max_correction = 340,
    .max_motor_cmd = 900,
    .motor_deadband = 120,

    /* 丢线恢复：决定离线后原地找线力度和最长允许时间。 */
    .recover_turn_speed = 220,
    .recover_timeout_ms = 900U,

    /* 雷达判定：距离单位统一为 mm，实机需确认 LD2410S 波特率和距离单位。 */
    .radar_enable = true,
    .radar_uart_baudrate = 256000U,
    .radar_trigger_distance_mm = 450U,
    .radar_release_distance_mm = 650U,
    .radar_debounce_frames = 3U,
    .radar_frame_timeout_ms = 120U,

    /* 避障动作：定时开环绕障，上板主要调 turn/bypass/reacquire 时间和速度。 */
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
};
