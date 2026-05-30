#ifndef LF_CONFIG_H
#define LF_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * 巡线一期参数集中管理文件
 * 说明：
 * 1. 所有需要现场调参的量都收敛在 LF_Config 中，便于实验记录和版本管理。
 * 2. 建议每次只调整 1~2 个参数，并在实验日志中记录“参数 -> 结果”映射。
 */

#define LF_SENSOR_COUNT (8U)

typedef enum {
    /* 直接采集每路模拟量（ADC）。 */
    LF_SENSOR_INPUT_ANALOG_ADC = 0,

    /* 8 路 GPIO 高低电平（Yahboom 8-LP 的 10pin IO 模式）。 */
    LF_SENSOR_INPUT_DIGITAL_GPIO,

    /* 串口协议读取（Yahboom 8-LP 的 4pin UART 模式，后续扩展）。 */
    LF_SENSOR_INPUT_UART_PROTOCOL,

    /* I2C 协议读取（Yahboom 8-LP 的 4pin I2C 模式，后续扩展）。 */
    LF_SENSOR_INPUT_I2C_PROTOCOL,
} LF_SensorInputMode;

typedef struct {
    /* 主循环节拍（ms）。建议保持固定周期执行控制算法。 */
    uint16_t control_period_ms;

    /* 自动开始延时（ms）。上电后等待该时间进入标定。 */
    uint32_t auto_start_delay_ms;

    /* 光照标定持续时间（ms）。 */
    uint32_t calibration_duration_ms;

    /* 标定阶段左右转向切换周期（ms）。用于让传感器尽量扫过黑/白区域。 */
    uint16_t calibration_switch_interval_ms;

    /* 标定阶段原地转向速度（-1000~1000）。 */
    int16_t calibration_spin_speed;

    /* 低通滤波系数（0~1）。值越大越灵敏，值越小越平滑。 */
    float sensor_filter_alpha;

    /* 传感器输入模式（支持 8-LP 的 GPIO/UART/I2C 三种通信接口）。 */
    LF_SensorInputMode sensor_input_mode;

    /* 模拟量极性翻转（当“黑线原始值更小”时可置 true）。 */
    bool sensor_invert_polarity;

    /* GPIO 数字模式阈值（raw >= threshold 视为高电平）。 */
    uint16_t sensor_digital_threshold;

    /* GPIO 数字模式下“在线”有效电平：true=高电平有效，false=低电平有效。 */
    bool sensor_digital_active_high;

    /* 是否启用动态标定（数字模式建议关闭）。 */
    bool sensor_use_dynamic_calibration;

    /* 标定降级：少量通道异常时允许限速运行。 */
    bool sensor_allow_degraded_calibration;
    uint8_t sensor_min_valid_count;
    uint16_t sensor_calibration_min_delta;
    int16_t sensor_degraded_max_speed;

    /* 判定“在线上”的最小强度和。 */
    uint16_t line_detect_min_sum;
    uint16_t line_detect_min_peak;
    uint16_t line_detect_min_contrast;
    uint8_t line_lost_grace_ticks;

    /* 左右半区强度差超过该阈值时，更新丢线恢复方向。 */
    uint16_t edge_hint_threshold;

    /* 传感器横向权重。左负右正。 */
    int16_t sensor_weights[LF_SENSOR_COUNT];

    /* 巡线 PID 参数。 */
    float kp;
    float ki;
    float kd;

    /* 基础速度（-1000~1000）。 */
    int16_t base_speed;

    /* PID 输出限幅，避免急剧打角。 */
    int16_t max_correction;
    int16_t adaptive_slow_speed;
    int16_t adaptive_error_threshold;
    float adaptive_confidence_threshold;
    int16_t sharp_turn_speed;

    bool straight_boost_enable;
    bool curve_prepare_enable;
    bool line_stability_enable;
    bool stable_direction_enable;
    int16_t straight_boost_speed;
    int16_t straight_error_threshold;
    int16_t straight_delta_threshold;
    float straight_confidence_min;
    uint8_t straight_confirm_ticks;
    int16_t curve_prepare_speed;
    int16_t curve_prepare_error_threshold;
    int16_t curve_prepare_delta_threshold;
    uint8_t curve_prepare_confirm_ticks;
    uint8_t interference_active_count_threshold;
    int16_t interference_position_jump_threshold;
    float direction_update_confidence_min;

    int16_t line_hold_speed;
    int16_t line_hold_turn_speed;
    float derivative_filter_alpha;
    float integral_limit;
    int16_t max_output_delta_per_tick;

    /* 电机命令绝对值上限。 */
    int16_t max_motor_cmd;

    /* 电机最小起转补偿（非零命令时生效）。 */
    int16_t motor_deadband;

    /* 丢线恢复参数。 */
    int16_t recover_turn_speed;
    int16_t recover_backtrack_speed;
    uint16_t recover_backtrack_ms;
    int16_t recover_sweep_start_speed;
    int16_t recover_sweep_max_speed;
    uint16_t recover_confirm_ticks;
    float recover_confidence_min;
    uint32_t recover_timeout_ms;

    /* 雷达串口参数与避障阈值（HLK-LD2410S，默认值待实机确认）。 */
    bool radar_enable;
    uint32_t radar_uart_baudrate;
    uint16_t radar_trigger_distance_mm;
    uint16_t radar_release_distance_mm;
    uint8_t radar_debounce_frames;
    uint16_t radar_frame_timeout_ms;

    /* 避障减速速度（WARN 状态下生效）。 */
    int16_t obstacle_warn_speed;
    bool obstacle_avoid_enable;
    int8_t obstacle_preferred_side;
    uint8_t obstacle_max_attempts;
    uint16_t obstacle_confirm_ms;
    uint16_t obstacle_stop_ms;
    uint16_t obstacle_turn_out_ms;
    uint16_t obstacle_turn_out_min_ms;
    uint16_t obstacle_bypass_ms;
    uint16_t obstacle_bypass_min_ms;
    uint16_t obstacle_turn_in_ms;
    uint16_t obstacle_turn_in_min_ms;
    uint16_t obstacle_reacquire_timeout_ms;
    uint16_t obstacle_safe_distance_mm;
    uint16_t obstacle_line_reacquire_min_sum;
    uint8_t obstacle_reacquire_confirm_ticks;
    int16_t obstacle_turn_speed;
    int16_t obstacle_bypass_inner_speed;
    int16_t obstacle_bypass_outer_speed;
    uint16_t obstacle_emergency_distance_mm;

    /* 岔路决策：左侧斜装雷达只在岔路窗口内解释为左支路占用检测。 */
    bool fork_enable;
    uint16_t fork_detect_min_sum;
    uint16_t fork_sensor_active_threshold;
    uint8_t fork_detect_min_active_sensors;
    uint16_t fork_left_min_sum;
    uint16_t fork_right_min_sum;
    int32_t fork_detect_max_abs_position;
    uint8_t fork_detect_confirm_ticks;
    uint16_t fork_sample_ms;
    uint16_t fork_radar_max_age_ms;
    uint16_t fork_radar_min_distance_mm;
    uint16_t fork_radar_block_distance_mm;
    uint8_t fork_radar_block_confirm_frames;
    uint8_t fork_radar_valid_min_samples;
    int8_t fork_fallback_branch;
    int16_t fork_sample_speed;
    int16_t fork_commit_speed;
    int16_t fork_commit_turn_speed;
    uint16_t fork_commit_min_ms;
    uint16_t fork_commit_ms;
    uint16_t fork_reacquire_timeout_ms;
    uint16_t fork_line_reacquire_min_sum;
    uint8_t fork_reacquire_confirm_ticks;
    uint16_t fork_cooldown_ms;

    /* 无旋转快速标定：跳过原地旋转，用固定 min/max。适合无按钮独立运行。 */
    bool sensor_fast_calibration;
} LF_Config;

/* 全局可变参数实例。可在 main 中直接修改，或通过 lf_config_profiles 预设覆盖。 */
extern LF_Config g_lf_config;

void LF_Config_ApplyDebugProfile(void);
void LF_Config_ApplyTrackProfile(void);
void LF_Config_ApplyCompetitionProfile(void);

#endif /* LF_CONFIG_H */
