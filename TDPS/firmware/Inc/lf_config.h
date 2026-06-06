/**
 * @file lf_config.h
 * @brief 巡线小车参数集中管理
 *
 * 所有现场可调参数统一收敛到 LF_Config 结构体中，便于实验记录和版本管理。
 * 建议每次仅调整 1~2 个参数，并在实验日志中记录"参数 → 结果"映射。
 */
#ifndef LF_CONFIG_H
#define LF_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* 传感器通道数（Yahboom 8-LP 巡线模块） */
#define LF_SENSOR_COUNT  (8U)

/* ==================== 枚举定义 ==================== */

/** 传感器输入模式 */
typedef enum {
    LF_SENSOR_INPUT_ANALOG_ADC = 0,     /**< ADC 模拟量采集 */
    LF_SENSOR_INPUT_DIGITAL_GPIO,       /**< GPIO 高低电平 */
    LF_SENSOR_INPUT_UART_PROTOCOL,      /**< 串口协议（Yahboom 4pin UART） */
    LF_SENSOR_INPUT_I2C_PROTOCOL,       /**< I2C 协议（预留） */
} LF_SensorInputMode;

/**
 * @brief 路段类型
 *
 * 由 detect_segment_type() 每帧更新，供分段参数切换和策略选择使用。
 * 优先级：丢线 > 宽线/路口 > 急弯 > 缓弯 > 直道。
 */
typedef enum {
    LF_SEGMENT_STRAIGHT = 0,      /**< 直道：2~3 通道活跃，|pos| 小且稳定 */
    LF_SEGMENT_GENTLE_CURVE,      /**< 缓弯：position 渐偏，一侧活跃递增 */
    LF_SEGMENT_TIGHT_CURVE,       /**< 急弯/连续弯：position 大偏移 */
    LF_SEGMENT_WIDE_LINE,         /**< 宽线/路口/直角入口：大面积亮 */
    LF_SEGMENT_LOST,              /**< 丢线 */
} LF_SegmentType;

/* ==================== 参数结构体 ==================== */

/**
 * @brief 巡线参数集合
 *
 * 参数分组：
 * - 启动与标定：上电等待和校准流程
 * - 传感器：滤波、标定、极性、阈值、权重
 * - 巡线控制：PD + kff 核心参数
 * - 电机：死区、限幅
 * - 丢线恢复：自救参数
 * - 雷达避障：触发阈值和绕行参数
 * - 直角弯：原地旋转参数
 * - 路线脚本：固定赛道导航
 * - 固定转弯：90° 原地旋转
 * - 分段控制：路段类型切换
 */
typedef struct {
    /* ==================== 启动与标定 ==================== */

    uint16_t control_period_ms;            /**< 主循环节拍（ms），建议固定 10 */
    uint32_t auto_start_delay_ms;          /**< 自动启动延时（ms），0 = 禁用倒计时 */
    uint32_t start_min_boot_delay_ms;      /**< 无按钮启动：最小上电等待（ms） */
    uint32_t start_line_hold_ms;           /**< 无按钮启动：检测到线持续时长（ms） */
    uint32_t calibration_duration_ms;      /**< 光照标定持续时间（ms） */
    uint16_t calibration_switch_interval_ms; /**< 标定阶段转向切换周期（ms） */
    int16_t  calibration_spin_speed;       /**< 标定阶段原地转向速度 */
    bool     sensor_fast_calibration;      /**< 快速标定：跳过旋转，用固定 min/max */

    /* ==================== 传感器 ==================== */

    LF_SensorInputMode sensor_input_mode;  /**< 传感器输入模式 */
    bool     sensor_invert_polarity;       /**< 模拟量极性翻转（黑线值更小时置 true） */
    uint16_t sensor_digital_threshold;     /**< GPIO 数字模式阈值 */
    bool     sensor_digital_active_high;   /**< GPIO 有效电平：true = 高电平有效 */
    bool     sensor_use_dynamic_calibration; /**< 是否启用动态标定 */
    bool     sensor_allow_degraded_calibration; /**< 允许降级标定运行 */
    uint8_t  sensor_min_valid_count;       /**< 降级标定最少有效通道数 */
    uint16_t sensor_calibration_min_delta; /**< 标定最小动态范围 */
    int16_t  sensor_degraded_max_speed;    /**< 标定降级时限速 */
    float    sensor_filter_alpha;          /**< IIR 低通系数（0~1，越大越灵敏） */
    bool     sensor_edge_noise_reject_enable; /**< 边缘噪声抑制（端点异常亮时清零） */
    uint16_t sensor_edge_noise_neighbor_threshold; /**< 边缘噪声邻居阈值 */
    int16_t  sensor_weights[LF_SENSOR_COUNT]; /**< 传感器横向权重（左负右正） */

    /* ==================== 巡线检测 ==================== */

    uint16_t line_detect_min_sum;          /**< 在线最小信号总量 */
    uint16_t line_detect_min_peak;         /**< 在线最小峰值 */
    uint16_t line_detect_min_contrast;     /**< 在线最小对比度 */
    uint8_t  line_lost_grace_ticks;        /**< 丢线宽容帧数（保持方向滑行） */
    uint16_t edge_hint_threshold;          /**< 半区强度差方向提示阈值 */

    /* ==================== 巡线控制（PD + kff） ==================== */

    float    kp;                           /**< 比例增益 */
    float    kd;                           /**< 微分增益 */
    float    kff;                          /**< 曲率前馈增益（kff * derivative * speed） */
    int16_t  base_speed;                   /**< 基础前进速度 */
    int16_t  min_speed;                    /**< 弯道最低速度 */
    int16_t  max_correction;               /**< PD 输出限幅 */
    int8_t   steering_dir_sign;            /**< 转向符号：+1 正常，-1 倒三轮底盘 */
    float    derivative_filter_alpha;      /**< D 项一阶低通系数 */
    int16_t  max_output_delta_per_tick;    /**< 单拍修正变化上限（防急转） */
    int16_t  max_motor_cmd;                /**< 电机命令绝对值上限 */
    int16_t  max_motor_delta;              /**< 左右轮最大差速限制 */
    int16_t  motor_deadband;               /**< 电机最小起转补偿（非零时生效） */

    /* ==================== 丢线恢复 ==================== */

    int16_t  recover_forward_speed;        /**< 恢复阶段前进速度 */
    int16_t  recover_backtrack_speed;      /**< 恢复阶段后退速度 */
    uint16_t recover_forward_ms;           /**< 前冲持续时间（ms） */
    uint16_t recover_backtrack_ms;         /**< 后退持续时间（ms） */
    int16_t  recover_sweep_speed;          /**< 左右扫描速度 */
    uint16_t recover_sweep_ms;             /**< 单侧扫描时间（ms） */
    uint32_t recover_timeout_ms;           /**< 恢复总超时（ms），超时停车 */

    /* ==================== 雷达避障 ==================== */

    bool     radar_enable;                 /**< 启用雷达模块 */
    uint32_t radar_uart_baudrate;          /**< 雷达串口波特率 */
    uint16_t radar_trigger_distance_mm;    /**< 避障触发距离（mm） */
    uint16_t radar_release_distance_mm;    /**< 避障解除距离（mm） */
    uint8_t  radar_debounce_frames;        /**< 雷达状态去抖帧数 */
    uint16_t radar_frame_timeout_ms;       /**< 雷达数据帧超时（ms） */
    bool     obstacle_avoid_enable;        /**< 启用避障动作 */
    int8_t   obstacle_preferred_side;      /**< 避障优先方向：-1 左，0 自动，+1 右 */
    uint16_t obstacle_stop_ms;             /**< 避障停车确认时间（ms） */
    uint16_t obstacle_bypass_ms;           /**< 绕行持续时间（ms） */
    uint16_t obstacle_reacquire_timeout_ms; /**< 绕行后重新找线超时（ms） */
    int16_t  obstacle_turn_speed;          /**< 避障转向速度 */
    int16_t  obstacle_bypass_speed;        /**< 绕行直行速度 */

    /* ==================== 直角弯（原地旋转对准） ==================== */

    bool     reorient_enable;              /**< 启用直角弯原地旋转 */
    int16_t  reorient_spin_speed;          /**< 原地旋转速度 */
    uint16_t reorient_timeout_ms;          /**< 旋转超时（ms） */
    int16_t  reorient_position_threshold;  /**< 触发旋转的最小位置偏移 */
    uint16_t reorient_cooldown_ms;         /**< 旋转完成后冷却期（ms），防 U 弯振荡 */
    uint8_t  reorient_confirm_ticks;       /**< 中间传感器对准确认帧数 */

    /* ==================== 路线脚本 ==================== */

    bool     route_script_enable;          /**< 启用路线脚本（固定赛道导航） */
    uint8_t  route_event_confirm_ticks;    /**< 路口事件确认帧数 */
    uint16_t route_event_cooldown_ms;      /**< 路口事件冷却时间（ms） */

    /* ==================== 固定 90° 转弯 ==================== */

    bool     fixed_turn_enable;            /**< 启用固定 90° 原地旋转 */
    int16_t  fixed_turn_spin_speed;        /**< 固定转弯旋转速度 */
    uint16_t fixed_turn_stop_ms;           /**< 转弯前停车等待（ms） */
    uint16_t fixed_turn_90_ms_left;        /**< 左转 90° 持续时间（ms） */
    uint16_t fixed_turn_90_ms_right;       /**< 右转 90° 持续时间（ms） */
    uint16_t fixed_turn_settle_ms;         /**< 转弯后稳定时间（ms） */
    uint16_t fixed_turn_cooldown_ms;       /**< 转弯冷却期（ms） */

    /* ==================== 分段控制 ==================== */

    bool     segment_control_enable;       /**< 启用分段参数切换 */
    uint8_t  segment_confirm_ticks;        /**< 路段切换确认帧数 */
    uint8_t  segment_hold_ticks;           /**< 路段最少保持帧数（防止抖动） */

    /* 直道参数 */
    float    seg_kp_straight;
    float    seg_kd_straight;
    float    seg_kff_straight;
    int16_t  seg_base_speed_straight;
    int16_t  seg_min_speed_straight;
    int16_t  seg_max_correction_straight;

    /* 弯道参数（缓弯/急弯共用，通过 IIR 平滑过渡） */
    float    seg_kp_curve;
    float    seg_kd_curve;
    float    seg_kff_curve;
    int16_t  seg_base_speed_curve;
    int16_t  seg_min_speed_curve;
    int16_t  seg_max_correction_curve;
} LF_Config;

/* 全局可变参数实例。可在 main 中直接修改，或通过 LF_Config_ApplyDebugProfile 预设覆盖。 */
extern LF_Config g_lf_config;

/** 应用调试配置（保守低速参数，适合首次调试和安全验证） */
void LF_Config_ApplyDebugProfile(void);

#endif /* LF_CONFIG_H */
