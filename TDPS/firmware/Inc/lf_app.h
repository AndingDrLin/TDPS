/**
 * @file lf_app.h
 * @brief 巡线应用层状态机
 *
 * 状态流转：
 *   WAIT_START → CALIBRATING → RUNNING
 *   RUNNING ⇄ RECOVERING
 *   RUNNING → AVOID_* → RUNNING
 *   RUNNING → FIXED_TURN_* → RUNNING
 *   RUNNING → REORIENT_* → RUNNING
 */
#ifndef LF_APP_H
#define LF_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_config.h"
#include "lf_control.h"
#include "lf_radar.h"
#include "lf_sensor.h"

/** 应用主状态 */
typedef enum {
    LF_APP_STATE_BOOT = 0,
    LF_APP_STATE_WAIT_START,
    LF_APP_STATE_CALIBRATING,
    LF_APP_STATE_RUNNING,
    LF_APP_STATE_RECOVERING,
    LF_APP_STATE_AVOID_PREP,
    LF_APP_STATE_AVOID_BYPASS,
    LF_APP_STATE_AVOID_REACQUIRE,
    LF_APP_STATE_FIXED_TURN_STOP,
    LF_APP_STATE_FIXED_TURN_SPIN,
    LF_APP_STATE_FIXED_TURN_SETTLE,
    LF_APP_STATE_REORIENT_STOP,
    LF_APP_STATE_REORIENT_SPIN,
    LF_APP_STATE_REORIENT_CONFIRM,
    LF_APP_STATE_STOPPED,
    LF_APP_STATE_FAULT,
} LF_AppState;

/** 固定路线阶段 */
typedef enum {
    LF_ROUTE_PHASE_WAIT_ARM = 0,       /**< S 弯后第一个 T 口 → 右转 */
    LF_ROUTE_PHASE_T_RIGHT,            /**< 兼容旧阶段：T 口 → 右转 */
    LF_ROUTE_PHASE_LEFT_1,             /**< 左直角 → 左转 */
    LF_ROUTE_PHASE_CROSS,              /**< 十字 ×2 → 直行 */
    LF_ROUTE_PHASE_LEFT_2,             /**< 左直角 → 左转 */
    LF_ROUTE_PHASE_RIGHT_1,            /**< 右直角 → 右转 */
    LF_ROUTE_PHASE_RIGHT_STRAIGHT,     /**< 右直角 → 直行 */
    LF_ROUTE_PHASE_RIGHT_2,            /**< 右直角 → 右转 */
    LF_ROUTE_PHASE_T_LEFT,             /**< T 口 → 左转 */
    LF_ROUTE_PHASE_LEFT_3,             /**< 左直角 → 左转 */
    LF_ROUTE_PHASE_DONE,
} LF_RoutePhase;

/** 固定转弯事件，仅用于调试观察 */
typedef enum {
    LF_ROUTE_EVENT_NONE = 0,
    LF_ROUTE_EVENT_FIRST_T_RIGHT,
    LF_ROUTE_EVENT_LEFT_RIGHT_ANGLE,
    LF_ROUTE_EVENT_RIGHT_RIGHT_ANGLE,
    LF_ROUTE_EVENT_T_LEFT,
} LF_RouteEvent;

/** 丢线恢复阶段 */
typedef enum {
    LF_RECOVER_FORWARD = 0,
    LF_RECOVER_BACKTRACK,
    LF_RECOVER_SWEEP_LEFT,
    LF_RECOVER_SWEEP_RIGHT,
    LF_RECOVER_CONFIRM,
} LF_RecoverPhase;

/**
 * @brief 应用层上下文
 *
 * 字段只保留状态机真正需要的运行数据。Keil Watch 可直接观察本结构体，
 * 不再散落大量全局调试变量。
 */
typedef struct {
    /* 基本状态 */
    LF_AppState state;
    uint32_t boot_ms;
    uint32_t last_step_us;
    uint32_t wait_start_line_since_ms;
    uint32_t calib_start_ms;
    uint32_t run_start_ms;
    bool calibration_ok;
    bool calibration_degraded;
    bool run_finalized;

    /* 巡线控制 */
    LF_PIDState pid;
    LF_SensorFrame last_frame;
    int16_t current_target_speed;
    int8_t last_seen_dir;
    int8_t trusted_line_dir;
    int32_t last_trusted_position;
    bool trusted_line_valid;
    uint8_t line_lost_count;

    /* 雷达快照 */
    LF_RadarObstacleState obstacle_state;
    uint16_t obstacle_distance_mm;
    bool radar_has_target;
    bool radar_frame_valid;
    uint32_t radar_frame_count;
    uint32_t radar_last_update_ms;
    uint32_t radar_frame_age_ms;

    /* 丢线恢复 */
    uint32_t recover_start_ms;
    uint32_t recover_phase_start_ms;
    uint8_t recover_phase;
    uint8_t recover_confirm_count;
    uint16_t recovery_count;

    /* 避障 */
    int8_t avoid_dir;
    uint32_t avoid_state_start_ms;
    uint8_t avoid_confirm_count;

    /* 固定 90° 转弯 */
    uint32_t fixed_turn_phase_start_ms;
    int8_t fixed_turn_dir;
    uint8_t fixed_turn_event;
    uint32_t route_last_event_ms;

    /* 直角弯兜底旋转 */
    int8_t reorient_spin_dir;
    uint32_t reorient_start_ms;
    uint32_t reorient_last_finish_ms;
    uint8_t reorient_confirm_count;

    /* 固定路线脚本 */
    uint8_t route_phase;
    uint8_t route_cross_count;
    uint8_t route_event_confirm_count;
    bool route_cross_armed;

    /* 分段控制 */
    LF_SegmentType segment_type;
    LF_SegmentType segment_candidate;
    uint8_t segment_candidate_count;
    uint8_t segment_hold_count;
    int8_t position_sign_history[20];
    uint8_t sign_history_index;
    uint8_t direction_switch_count;
} LF_AppContext;

void LF_App_Init(void);
void LF_App_RunStep(void);
void LF_App_NotifyCheckpoint(uint32_t checkpoint_id);
const LF_AppContext *LF_App_GetContext(void);

#endif /* LF_APP_H */
