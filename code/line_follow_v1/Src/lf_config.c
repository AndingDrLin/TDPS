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
    .sensor_input_mode = LF_SENSOR_INPUT_ANALOG_ADC,
    .sensor_invert_polarity = false,
    .sensor_digital_threshold = 2048U,
    /* TODO(8-LP): 实机确认 X1~X8 黑线有效电平，必要时改为 true。 */
    .sensor_digital_active_high = false,
    .sensor_use_dynamic_calibration = true,
    .line_detect_min_sum = 780U,
    .sensor_weights = {-1750, -1250, -750, -250, 250, 750, 1250, 1750},
    
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

    /* TODO: 根据 HLK-LD2410S 实机固件参数确认默认波特率与触发距离。 */
    .radar_enable = true,
    .radar_uart_baudrate = 256000U,
    .radar_trigger_distance_mm = 450U,
    .radar_release_distance_mm = 650U,
    .radar_debounce_frames = 3U,
    .radar_frame_timeout_ms = 120U,

    .obstacle_warn_speed = 220,
};
