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

    /* 判定“在线上”的最小强度和。 */
    uint16_t line_detect_min_sum;

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

    /* 电机命令绝对值上限。 */
    int16_t max_motor_cmd;

    /* 电机最小起转补偿（非零命令时生效）。 */
    int16_t motor_deadband;

    /* 丢线恢复参数。 */
    int16_t recover_turn_speed;
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
} LF_Config;

/* 全局只读参数实例。 */
extern const LF_Config g_lf_config;

#endif /* LF_CONFIG_H */
