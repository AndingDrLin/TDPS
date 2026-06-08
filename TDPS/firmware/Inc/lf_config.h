/**
 * @file lf_config.h
 * @brief Centralized parameter management for the line-following car.
 *
 * All field-adjustable parameters are consolidated into the LF_Config struct
 * for easy experiment logging and version control.
 * It is recommended to adjust only 1-2 parameters at a time and record
 * the "parameter -> result" mapping in the experiment log.
 */
#ifndef LF_CONFIG_H
#define LF_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* Number of sensor channels (Yahboom 8-LP line-following module). */
#define LF_SENSOR_COUNT  (8U)

/* ==================== Enum Definitions ==================== */

/** Sensor input mode. */
typedef enum {
    LF_SENSOR_INPUT_ANALOG_ADC = 0,     /**< ADC analog acquisition */
    LF_SENSOR_INPUT_DIGITAL_GPIO,       /**< GPIO high/low level */
    LF_SENSOR_INPUT_UART_PROTOCOL,      /**< UART protocol (Yahboom 4-pin UART) */
    LF_SENSOR_INPUT_I2C_PROTOCOL,       /**< I2C protocol (reserved) */
} LF_SensorInputMode;

/**
 * @brief Road segment type
 *
 * Updated every frame by detect_segment_type(), used for per-segment
 * parameter switching and strategy selection.
 * Priority: lost line > wide line/intersection > tight curve > gentle curve > straight.
 */
typedef enum {
    LF_SEGMENT_STRAIGHT = 0,      /**< Straight: 2-3 active channels, small and stable |pos| */
    LF_SEGMENT_GENTLE_CURVE,      /**< Gentle curve: position gradually shifts, one side increasing */
    LF_SEGMENT_TIGHT_CURVE,       /**< Tight/continuous curve: large position offset */
    LF_SEGMENT_WIDE_LINE,         /**< Wide line/intersection/right-angle entry: large bright area */
    LF_SEGMENT_LOST,              /**< Lost line */
} LF_SegmentType;

/* ==================== Parameter Structure ==================== */

/**
 * @brief Line-following parameter set
 *
 * Parameter groups:
 * - Startup and calibration: power-on wait and calibration procedure
 * - Sensor: filtering, calibration, polarity, thresholds, weights
 * - Line-following control: PD + kff core parameters
 * - Motor: dead zone, output clamping
 * - Lost-line recovery: self-rescue parameters
 * - Radar obstacle avoidance: trigger thresholds and bypass parameters
 * - Right-angle turn: in-place rotation parameters
 * - Route script: fixed-course navigation
 * - Fixed turn: 90-degree in-place rotation
 * - Segment control: road-segment type switching
 */
typedef struct {
    /* ==================== Startup and Calibration ==================== */

    uint16_t control_period_ms;            /**< Main loop period (ms), recommended fixed at 10 */
    uint32_t auto_start_delay_ms;          /**< Auto-start delay (ms), 0 = disable countdown */
    uint32_t start_min_boot_delay_ms;      /**< Buttonless start: minimum power-on wait (ms) */
    uint32_t start_line_hold_ms;           /**< Buttonless start: line detection hold duration (ms) */
    uint32_t calibration_duration_ms;      /**< Light calibration duration (ms) */
    uint16_t calibration_switch_interval_ms; /**< Calibration phase turn-switching period (ms) */
    int16_t  calibration_spin_speed;       /**< Calibration phase in-place rotation speed */
    bool     sensor_fast_calibration;      /**< Fast calibration: skip rotation, use fixed min/max */

    /* ==================== Sensor ==================== */

    LF_SensorInputMode sensor_input_mode;  /**< Sensor input mode */
    bool     sensor_invert_polarity;       /**< Analog polarity inversion (set true when black-line value is smaller) */
    uint16_t sensor_digital_threshold;     /**< GPIO digital mode threshold */
    bool     sensor_digital_active_high;   /**< GPIO active level: true = active high */
    bool     sensor_use_dynamic_calibration; /**< Enable dynamic calibration */
    bool     sensor_allow_degraded_calibration; /**< Allow degraded calibration to proceed */
    uint8_t  sensor_min_valid_count;       /**< Minimum valid channel count for degraded calibration */
    uint16_t sensor_calibration_min_delta; /**< Minimum dynamic range for calibration */
    int16_t  sensor_degraded_max_speed;    /**< Speed limit during degraded calibration */
    float    sensor_filter_alpha;          /**< IIR low-pass coefficient (0-1, higher = more responsive) */
    bool     sensor_edge_noise_reject_enable; /**< Edge noise suppression (zero out when endpoint is abnormally bright) */
    uint16_t sensor_edge_noise_neighbor_threshold; /**< Edge noise neighbor threshold */
    int16_t  sensor_weights[LF_SENSOR_COUNT]; /**< Sensor lateral weights (negative = left, positive = right) */

    /* ==================== Line Detection ==================== */

    uint16_t line_detect_min_sum;          /**< Minimum signal sum for on-line */
    uint16_t line_detect_min_peak;         /**< Minimum peak value for on-line */
    uint16_t line_detect_min_contrast;     /**< Minimum contrast for on-line */
    uint8_t  line_lost_grace_ticks;        /**< Lost-line grace frames (coast while holding direction) */
    uint16_t edge_hint_threshold;          /**< Half-region intensity difference direction hint threshold */

    /* ==================== Line-Following Control (PD + kff) ==================== */

    float    kp;                           /**< Proportional gain */
    float    kd;                           /**< Derivative gain */
    float    kff;                          /**< Curvature feedforward gain (kff * derivative * speed) */
    int16_t  base_speed;                   /**< Base forward speed */
    int16_t  min_speed;                    /**< Minimum curve speed */
    int16_t  max_correction;               /**< PD output clamp */
    int8_t   steering_dir_sign;            /**< Steering direction sign: +1 normal, -1 for reversed rear-steer chassis */
    float    derivative_filter_alpha;      /**< D-term first-order low-pass coefficient */
    int16_t  max_output_delta_per_tick;    /**< Per-tick correction change limit (prevents sudden turns) */
    int16_t  max_motor_cmd;                /**< Motor command absolute value limit */
    int16_t  max_motor_delta;              /**< Left/right wheel maximum differential limit */
    int16_t  motor_deadband;               /**< Motor minimum startup compensation (active when non-zero) */

    /* ==================== Lost-Line Recovery ==================== */

    int16_t  recover_forward_speed;        /**< Recovery phase forward speed */
    int16_t  recover_backtrack_speed;      /**< Recovery phase backward speed */
    uint16_t recover_forward_ms;           /**< Forward dash duration (ms) */
    uint16_t recover_backtrack_ms;         /**< Backward duration (ms) */
    int16_t  recover_sweep_speed;          /**< Left/right scan speed */
    uint16_t recover_sweep_ms;             /**< Single-side scan time (ms) */
    uint32_t recover_timeout_ms;           /**< Total recovery timeout (ms); stops on timeout */

    /* ==================== Radar Obstacle Avoidance ==================== */

    bool     radar_enable;                 /**< Enable radar module */
    uint32_t radar_uart_baudrate;          /**< Radar UART baud rate */
    uint16_t radar_trigger_distance_mm;    /**< Obstacle avoidance trigger distance (mm) */
    uint16_t radar_release_distance_mm;    /**< Obstacle avoidance release distance (mm) */
    uint8_t  radar_debounce_frames;        /**< Radar state debounce frame count */
    uint16_t radar_frame_timeout_ms;       /**< Radar data frame timeout (ms) */
    bool     obstacle_avoid_enable;        /**< Enable obstacle avoidance action */
    int8_t   obstacle_preferred_side;      /**< Preferred avoidance side: -1 left, 0 auto, +1 right */
    uint16_t obstacle_stop_ms;             /**< Obstacle stop confirmation time (ms) */
    uint16_t obstacle_bypass_ms;           /**< Bypass duration (ms) */
    uint16_t obstacle_reacquire_timeout_ms; /**< Line re-acquisition timeout after bypass (ms) */
    int16_t  obstacle_turn_speed;          /**< Obstacle avoidance turn speed */
    int16_t  obstacle_bypass_speed;        /**< Bypass straight-line speed */

    /* ==================== Right-Angle Turn (In-Place Rotation Alignment) ==================== */

    bool     reorient_enable;              /**< Enable right-angle in-place rotation */
    int16_t  reorient_spin_speed;          /**< In-place rotation speed */
    uint16_t reorient_timeout_ms;          /**< Rotation timeout (ms) */
    int16_t  reorient_position_threshold;  /**< Minimum position offset to trigger rotation */
    uint16_t reorient_cooldown_ms;         /**< Cooldown period after rotation (ms), prevents U-turn oscillation */
    uint8_t  reorient_confirm_ticks;       /**< Center sensor alignment confirmation frame count */

    /* ==================== Route Script ==================== */

    bool     route_script_enable;          /**< Enable route script (fixed-course navigation) */
    uint8_t  route_event_confirm_ticks;    /**< Intersection event confirmation frame count */
    uint16_t route_event_cooldown_ms;      /**< Intersection event cooldown time (ms) */

    /* ==================== Fixed 90-Degree Turn ==================== */

    bool     fixed_turn_enable;            /**< Enable fixed 90-degree in-place rotation */
    int16_t  fixed_turn_spin_speed;        /**< Fixed turn rotation speed */
    uint16_t fixed_turn_stop_ms;           /**< Pre-turn stop wait (ms) */
    uint16_t fixed_turn_90_ms_left;        /**< Left 90-degree turn duration (ms) */
    uint16_t fixed_turn_90_ms_right;       /**< Right 90-degree turn duration (ms) */
    uint16_t fixed_turn_settle_ms;         /**< Post-turn settling time (ms) */
    uint16_t fixed_turn_cooldown_ms;       /**< Turn cooldown period (ms) */

    /* ==================== Segment Control ==================== */

    bool     segment_control_enable;       /**< Enable per-segment parameter switching */
    uint8_t  segment_confirm_ticks;        /**< Segment switch confirmation frame count */
    uint8_t  segment_hold_ticks;           /**< Minimum segment hold frame count (prevents jitter) */

    /* Straight segment parameters */
    float    seg_kp_straight;
    float    seg_kd_straight;
    float    seg_kff_straight;
    int16_t  seg_base_speed_straight;
    int16_t  seg_min_speed_straight;
    int16_t  seg_max_correction_straight;

    /* Curve segment parameters (shared for gentle/tight curves, with IIR smooth transitions) */
    float    seg_kp_curve;
    float    seg_kd_curve;
    float    seg_kff_curve;
    int16_t  seg_base_speed_curve;
    int16_t  seg_min_speed_curve;
    int16_t  seg_max_correction_curve;

    /* Ultrasonic overhead obstacle detection */
    bool     ultrasonic_enable;               /**< Enable ultrasonic sensor */
    uint16_t ultrasonic_threshold_mm;         /**< Distance threshold in mm for overhead obstacle */
    uint16_t ultrasonic_hold_ms;              /**< Duration to stop and hold when obstacle detected */
    uint32_t ultrasonic_lora_checkpoint_id;   /**< LoRa checkpoint ID to send on detection */
    uint16_t ultrasonic_measure_interval_ms;  /**< Minimum interval between measurements */
    uint32_t ultrasonic_cooldown_ms;          /**< Cooldown after hold completes before re-triggering */
} LF_Config;

/* Global mutable parameter instance. Can be modified directly in main,
 * or overridden via LF_Config_ApplyDebugProfile. */
extern LF_Config g_lf_config;

/** @brief Apply the debug profile (conservative low-speed parameters, suitable for initial debugging and safe validation). */
void LF_Config_ApplyDebugProfile(void);

#endif /* LF_CONFIG_H */
