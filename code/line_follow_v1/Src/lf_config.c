#include "lf_config.h"

/*
 * 建议调参顺序：先 base_speed，再 kp，再 kd，最后视情况启用 ki。
 */

const LF_Config g_lf_config = {
    .control_period_ms = 10U,
    .auto_start_delay_ms = 1200U,
    .calibration_duration_ms = 3000U,
    .calibration_switch_interval_ms = 300U,
    .calibration_spin_speed = 180,
    .sensor_filter_alpha = 0.35f,
    .line_detect_min_sum = 700U,
    .sensor_weights = {-1500, -500, 500, 1500},
    
    // 需要调整的PID参数
    .kp = 0.42f,
    .ki = 0.0f,
    .kd = 1.65f,
    .base_speed = 360,

    .max_correction = 300,
    .max_motor_cmd = 900,
    .motor_deadband = 120,
    .recover_turn_speed = 220,
    .recover_timeout_ms = 900U,
};
