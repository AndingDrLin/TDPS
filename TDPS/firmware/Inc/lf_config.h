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

/*
 * 路段类型：用于分段控制，根据传感器模式 + 时序特征判断当前路段。
 * 每帧由 detect_segment_type() 更新，供参数切换和策略选择使用。
 */
typedef enum {
    LF_SEGMENT_STRAIGHT = 0,      /* 直道：2-3通道活跃，|pos|小且稳定 */
    LF_SEGMENT_GENTLE_CURVE,      /* 缓弯：position逐渐偏移，一侧活跃递增 */
    LF_SEGMENT_TIGHT_CURVE,       /* 急弯/连续弯：position大幅偏移，active>=4 */
    LF_SEGMENT_WIDE_LINE,         /* 宽线/路口/直角弯入口：大面积亮 */
    LF_SEGMENT_FORK,              /* 岔路口：两侧均有信号 */
    LF_SEGMENT_LOST,              /* 丢线中 */
} LF_SegmentType;

typedef struct {
    /* 主循环节拍（ms）。建议保持固定周期执行控制算法。 */
    uint16_t control_period_ms;

    /* 自动开始延时（ms）。0 表示禁用固定倒计时启动。 */
    uint32_t auto_start_delay_ms;

    /* 无按钮启动：上电安全等待后，检测到有效线持续一段时间再进入标定。 */
    uint32_t start_min_boot_delay_ms;
    uint32_t start_line_hold_ms;
    bool start_straight_guard_enable;
    uint32_t start_straight_guard_ms;
    int16_t start_straight_guard_speed;
    int16_t start_straight_guard_max_correction;
    uint8_t start_straight_guard_active_count;
    uint8_t start_straight_guard_release_ticks;
    uint8_t start_straight_guard_release_active_count;
    int16_t start_straight_guard_release_error;

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
    bool sensor_edge_noise_reject_enable;
    uint16_t sensor_edge_noise_neighbor_threshold;

    /* 传感器横向权重。左负右正。 */
    int16_t sensor_weights[LF_SENSOR_COUNT];

    /* 巡线 PID 参数。 */
    float kp;
    float ki;
    float kd;

    /* 基础速度（-1000~1000）。 */
    int16_t base_speed;

    /* 弯道最低速度（简化控制用）。 */
    int16_t min_speed;

    /* 曲率前馈增益（简化控制用）。 */
    float kff;
    int8_t steering_dir_sign;

    /* PID 输出限幅，避免急剧打角。 */
    int16_t max_correction;
    int16_t control_error_deadband;
    int16_t control_error_soft_zone;
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
    bool lead_compensation_enable;
    uint8_t lead_event_active_count_threshold;
    uint16_t lead_event_min_sum;
    int16_t lead_event_center_error_threshold;
    int16_t lead_event_entry_error_threshold;
    uint8_t lead_event_confirm_ticks;
    uint8_t lead_advance_ticks;
    int16_t lead_advance_speed;
    uint8_t lead_turn_hold_ticks;
    int16_t lead_turn_speed;
    int16_t lead_turn_delta;
    uint8_t lead_entry_memory_ticks;
    bool straight_noise_reject_enable;
    uint8_t straight_noise_confirm_ticks;
    uint8_t straight_noise_active_count_threshold;
    uint16_t straight_noise_max_sum;
    int16_t straight_noise_max_position_error;
    int16_t straight_noise_max_position_delta;

    int16_t line_hold_speed;
    int16_t line_hold_turn_speed;
    bool edge_realign_enable;
    int8_t edge_realign_dir_sign;
    int16_t edge_realign_speed;
    int16_t edge_realign_delta;
    uint8_t edge_realign_confirm_ticks;
    bool curve_arc_enable;
    int16_t curve_arc_speed;
    int16_t curve_arc_delta;
    int16_t curve_arc_max_motor_delta;
    uint8_t curve_arc_confirm_ticks;
    uint8_t curve_arc_release_ticks;

    /* 原地旋转对准：检测到急弯/直角弯时停车→原地转→对准中间传感器→恢复巡线。 */
    bool reorient_enable;
    int16_t reorient_spin_speed;
    uint8_t reorient_confirm_ticks;
    uint16_t reorient_timeout_ms;
    int16_t reorient_position_threshold;

    float derivative_filter_alpha;
    float integral_limit;
    /*
     * 积分分离 + 变速积分：
     * |error| > sep+soft → 积分指数衰减（防弯道记忆出弯过冲）
     * sep < |error| ≤ sep+soft → 积分速率线性降速（平滑过渡）
     * |error| ≤ sep → 全速积分（直线稳态修正）
     */
    float integral_separation_threshold;
    float integral_soft_zone;
    /*
     * PID 输出变化率限幅：单拍最大跳变量，防止传感器跳变导致急转。
     * 100Hz 控制频率下，80 意味着每秒最多变化 8000 单位 ≈ 全量程的 8 倍/s。
     */
    int16_t max_output_delta_per_tick;

    /* 电机命令绝对值上限。 */
    int16_t max_motor_cmd;
    int16_t max_motor_delta;

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

    /* ===== 分段控制 ===== */
    bool segment_control_enable;

    /* 路段检测 */
    uint8_t segment_confirm_ticks;     /* 路段切换确认帧数（默认3） */
    uint8_t segment_hold_ticks;        /* 路段最少保持帧数（默认8，防抖动） */
    uint8_t segment_history_len;       /* 时序历史长度（用于连续弯检测，默认20） */

    /* 分段参数：每种路段独立的 PD+kff 参数集 */
    float seg_kp_straight;
    float seg_kd_straight;
    float seg_kff_straight;
    int16_t seg_base_speed_straight;
    int16_t seg_min_speed_straight;
    int16_t seg_max_correction_straight;

    float seg_kp_gentle_curve;
    float seg_kd_gentle_curve;
    float seg_kff_gentle_curve;
    int16_t seg_base_speed_gentle_curve;
    int16_t seg_min_speed_gentle_curve;
    int16_t seg_max_correction_gentle_curve;

    float seg_kp_tight_curve;
    float seg_kd_tight_curve;
    float seg_kff_tight_curve;
    int16_t seg_base_speed_tight_curve;
    int16_t seg_min_speed_tight_curve;
    int16_t seg_max_correction_tight_curve;

    float seg_kp_wide_line;
    float seg_kd_wide_line;
    float seg_kff_wide_line;
    int16_t seg_base_speed_wide_line;
    int16_t seg_min_speed_wide_line;
    int16_t seg_max_correction_wide_line;

    float seg_kp_fork;
    float seg_kd_fork;
    float seg_kff_fork;
    int16_t seg_base_speed_fork;
    int16_t seg_min_speed_fork;
    int16_t seg_max_correction_fork;

    /* 连续弯 */
    uint8_t seg_curve_direction_window;      /* 方向切换检测窗口帧数（默认20） */
    uint8_t seg_curve_direction_switch_min;   /* 最小方向切换次数（默认2） */
    uint8_t seg_curve_grace_ticks_extra;      /* 连续弯额外宽容帧数（默认4，加到 line_lost_grace_ticks） */

    /* 直角转弯增强 */
    uint8_t right_angle_confirm_ticks;       /* 直角弯确认帧数（默认2） */
    int16_t reorient_approach_speed;         /* 直角弯靠近弯点速度（默认80） */
    uint16_t reorient_approach_ms;           /* 直角弯靠近弯点时间（默认100） */
    bool reorient_backtrack_enable;          /* 旋转超时后倒车重试（默认true） */
    int16_t reorient_backtrack_speed;        /* 倒车速度（默认120） */
    uint16_t reorient_backtrack_ms;          /* 倒车时间（默认400） */
    uint8_t reorient_max_retries;            /* 最大重试次数（默认2） */
} LF_Config;

/* 全局可变参数实例。可在 main 中直接修改，或通过 lf_config_profiles 预设覆盖。 */
extern LF_Config g_lf_config;

void LF_Config_ApplyDebugProfile(void);
void LF_Config_ApplyCompetitionProfile(void);

#endif /* LF_CONFIG_H */
