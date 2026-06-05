#include "lf_app.h"

#define TDPS_SIMPLE_CONTROL 1  /* 1=简化 6 参数 PD+kff，0=原始 PID+状态机 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#include "lf_chassis.h"
#include "lf_config.h"
#include "lf_control.h"
#include "lf_future_hooks.h"
#include "lf_led_blink.h"
#include "lf_platform.h"
#include "lf_radar.h"
#include "lf_run_log.h"
#include "lf_sensor.h"
#include "lf_watch_debug.h"

static LF_AppContext s_app;

static void update_led_for_state(void)
{
    switch (s_app.state) {
    case LF_APP_STATE_WAIT_START:
        LF_LedBlink_SetPattern(LF_LED_SLOW_BLINK);
        break;
    case LF_APP_STATE_CALIBRATING:
        LF_LedBlink_SetPattern(LF_LED_FAST_BLINK);
        break;
    case LF_APP_STATE_RUNNING:
        LF_LedBlink_SetPattern(LF_LED_ON);
        break;
    case LF_APP_STATE_RECOVERING:
        LF_LedBlink_SetPattern(LF_LED_TRIPLE_FLASH);
        break;
    case LF_APP_STATE_AVOID_PREP:
    case LF_APP_STATE_AVOID_TURN_OUT:
    case LF_APP_STATE_AVOID_BYPASS:
    case LF_APP_STATE_AVOID_TURN_IN:
    case LF_APP_STATE_AVOID_REACQUIRE:
        LF_LedBlink_SetPattern(LF_LED_HEARTBEAT);
        break;
    case LF_APP_STATE_FORK_SAMPLE:
    case LF_APP_STATE_FORK_COMMIT_LEFT:
    case LF_APP_STATE_FORK_COMMIT_RIGHT:
    case LF_APP_STATE_FORK_REACQUIRE:
        LF_LedBlink_SetPattern(LF_LED_RAPID_PULSE);
        break;
    case LF_APP_STATE_REORIENT_STOP:
    case LF_APP_STATE_REORIENT_APPROACH:
    case LF_APP_STATE_REORIENT_SPIN:
    case LF_APP_STATE_REORIENT_CONFIRM:
        LF_LedBlink_SetPattern(LF_LED_HEARTBEAT);
        break;
    default:
        break;
    }
}

static uint8_t decode_stop_reason(LF_AppReason reason)
{
    switch (reason) {
    case LF_APP_REASON_RECOVERY_TIMEOUT: return 1U;
    case LF_APP_REASON_AVOID_FAILED:     return 2U;
    case LF_APP_REASON_CALIBRATION_FAILED: return 3U;
    case LF_APP_REASON_FAULT_FALLBACK:   return 4U;
    default:                             return (uint8_t)reason;
    }
}

typedef enum {
    LF_RECOVER_BACKTRACK = 0,
    LF_RECOVER_SWEEP,
    LF_RECOVER_CONFIRM,
} LF_RecoverPhase;

static bool time_reached(uint32_t now, uint32_t last, uint32_t period)
{
    return ((uint32_t)(now - last) >= period);
}

static void set_state(LF_AppState state)
{
    if (s_app.state != state) {
        LF_RunLog_Record(LF_LOG_STATE_CHANGE, (uint8_t)state, (uint16_t)s_app.reason);
        s_app.state = state;
        update_led_for_state();
    }
}

static void set_reason(LF_AppReason reason)
{
    s_app.reason = reason;
}

static void enter_avoid_state(LF_AppState state, uint32_t now_ms)
{
    s_app.avoid_state_start_ms = now_ms;
    set_state(state);
}

static bool avoid_elapsed(uint32_t now_ms, uint32_t duration_ms)
{
    return (uint32_t)(now_ms - s_app.avoid_state_start_ms) >= duration_ms;
}

static int8_t choose_avoid_dir(void)
{
    if (g_lf_config.obstacle_preferred_side < 0) {
        return -1;
    }
    if (g_lf_config.obstacle_preferred_side > 0) {
        return +1;
    }
    return (s_app.last_seen_dir < 0) ? -1 : +1;
}

static void switch_avoid_dir(void)
{
    s_app.avoid_dir = (s_app.avoid_dir < 0) ? +1 : -1;
}

static void start_avoidance(uint32_t now_ms)
{
    LF_Chassis_Stop();
    LF_Hook_OnReservedObstacleWindow();
    LF_RunLog_Record(LF_LOG_OBSTACLE, 0U, s_app.obstacle_distance_mm);

    if (!g_lf_config.obstacle_avoid_enable) {
        return;
    }

    s_app.avoid_attempts = 0U;
    s_app.avoid_dir = choose_avoid_dir();
    enter_avoid_state(LF_APP_STATE_AVOID_PREP, now_ms);
}

static bool obstacle_emergency_stop(void)
{
    return s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK &&
           s_app.obstacle_distance_mm > 0U &&
           s_app.obstacle_distance_mm <= g_lf_config.obstacle_emergency_distance_mm;
}

static int16_t limit_speed_abs(int16_t speed, int16_t limit)
{
    if (limit <= 0) {
        return speed;
    }
    if (speed > limit) {
        return limit;
    }
    if (speed < (int16_t)(-limit)) {
        return (int16_t)(-limit);
    }
    return speed;
}

static int16_t limit_degraded_speed(int16_t speed)
{
    if (!s_app.calibration_degraded) {
        return speed;
    }
    return limit_speed_abs(speed, g_lf_config.sensor_degraded_max_speed);
}

static int32_t abs_i32(int32_t value);

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static void bump_counter(uint8_t *counter)
{
    if (counter != NULL && *counter < UINT8_MAX) {
        *counter += 1U;
    }
}

static void start_straight_guard_begin(uint32_t now_ms)
{
    s_app.run_start_ms = now_ms;
    s_app.start_straight_release_count = 0U;
    s_app.start_straight_guard_phase = g_lf_config.start_straight_guard_enable ?
        (uint8_t)LF_START_STRAIGHT_GUARD_ACTIVE :
        (uint8_t)LF_START_STRAIGHT_GUARD_OFF;
}

static bool start_straight_guard_tick(uint32_t now_ms, const LF_SensorFrame *frame)
{
    uint8_t release_ticks = g_lf_config.start_straight_guard_release_ticks;
    uint8_t release_active = g_lf_config.start_straight_guard_release_active_count;
    int16_t release_error = g_lf_config.start_straight_guard_release_error;

    if (!g_lf_config.start_straight_guard_enable ||
        s_app.start_straight_guard_phase != (uint8_t)LF_START_STRAIGHT_GUARD_ACTIVE ||
        frame == NULL || !frame->line_detected) {
        return false;
    }

    if (g_lf_config.start_straight_guard_ms > 0U &&
        (uint32_t)(now_ms - s_app.run_start_ms) >= g_lf_config.start_straight_guard_ms) {
        s_app.start_straight_guard_phase = (uint8_t)LF_START_STRAIGHT_GUARD_RELEASED;
        s_app.start_straight_release_count = 0U;
        LF_Control_ResetPid(&s_app.pid);
        return false;
    }

    if (release_ticks == 0U) {
        release_ticks = 1U;
    }
    if (release_active == 0U) {
        release_active = 1U;
    }
    if (release_error < 0) {
        release_error = (int16_t)(-release_error);
    }

    if (frame->active_count <= release_active &&
        abs_i32(frame->position) <= release_error) {
        bump_counter(&s_app.start_straight_release_count);
        if (s_app.start_straight_release_count >= release_ticks) {
            s_app.start_straight_guard_phase = (uint8_t)LF_START_STRAIGHT_GUARD_RELEASED;
            s_app.start_straight_release_count = 0U;
            LF_Control_ResetPid(&s_app.pid);
            return false;
        }
    } else {
        s_app.start_straight_release_count = 0U;
    }

    return true;
}

/* 前向声明：detect_segment_type 内部使用 */
static bool frame_is_interference(const LF_SensorFrame *frame);
static bool frame_is_straight_noise(const LF_SensorFrame *frame);
static bool frame_looks_like_fork(const LF_SensorFrame *frame);
static int8_t detect_curve_arc_side(const LF_SensorFrame *frame);
static int8_t detect_outer_gap_right_angle_side(const LF_SensorFrame *frame);
static int8_t detect_right_angle_turn_side(const LF_SensorFrame *frame);

/*
 * 路段检测：根据传感器特征 + 时序历史判断当前路段类型。
 * 优先级从高到低：丢线 > 岔路 > 宽线/路口 > 急弯 > 缓弯 > 直道。
 *
 * 滞回机制：
 * - 切换需要新候选连续 segment_confirm_ticks 帧一致才生效
 * - 切换后至少保持 segment_hold_ticks 帧防止抖动
 * - 保持期内除非丢线，否则不切换
 */
static void detect_segment_type(void)
{
    const LF_SensorFrame *frame = &s_app.last_frame;
    int32_t abs_pos;
    LF_SegmentType candidate = LF_SEGMENT_STRAIGHT;

    if (!frame->line_detected) {
        /* 丢线：立即切换，不经过候选期 */
        s_app.segment_type = LF_SEGMENT_LOST;
        s_app.segment_candidate = LF_SEGMENT_LOST;
        s_app.segment_candidate_count = 0U;
        s_app.segment_hold_count = g_lf_config.segment_hold_ticks;
        return;
    }

    abs_pos = abs_i32(frame->position);

    /* === 按优先级判定候选类型 === */

    /* 1. 岔路：复用已有 fork 检测 */
    if (g_lf_config.fork_enable && frame_looks_like_fork(frame)) {
        candidate = LF_SEGMENT_FORK;
    }
    /* 2. 宽线/路口：大面积亮且非直角弯（直角弯入口有一侧暗传感器，应优先判为急弯） */
    else if (frame->active_count >= 6U && frame->signal_sum >= 3500U &&
             !frame_is_interference(frame) && !frame_is_straight_noise(frame) &&
             detect_right_angle_turn_side(frame) == 0) {
        candidate = LF_SEGMENT_WIDE_LINE;
    }
    /* 3. 急弯/连续弯：检测到弯道弧线或直角转弯信号 + position 门限（降低到300匹配单暗传感器场景） */
    else if ((detect_curve_arc_side(frame) != 0 || detect_right_angle_turn_side(frame) != 0) &&
             abs_pos >= 300) {
        candidate = LF_SEGMENT_TIGHT_CURVE;
    }
    /* 4. 缓弯：position 偏了 或 edge_hint 非零 */
    else if (abs_pos >= 300 || frame->edge_hint != 0) {
        candidate = LF_SEGMENT_GENTLE_CURVE;
    }
    /* 5. 直道：以上都不满足 */
    else {
        candidate = LF_SEGMENT_STRAIGHT;
    }

    /* === 滞回判定 === */

    /* 保持期：除非丢线，否则不切换 */
    if (s_app.segment_hold_count > 0U) {
        s_app.segment_hold_count -= 1U;
        return;
    }

    /* 候选与当前相同 → 立即切换 */
    if (candidate == s_app.segment_type) {
        s_app.segment_candidate = candidate;
        s_app.segment_candidate_count = 0U;
        return;
    }

    /* 新候选需要连续确认 */
    if (candidate == s_app.segment_candidate) {
        s_app.segment_candidate_count += 1U;
        if (s_app.segment_candidate_count >= g_lf_config.segment_confirm_ticks) {
            s_app.segment_type = candidate;
            s_app.segment_hold_count = g_lf_config.segment_hold_ticks;
            s_app.segment_candidate_count = 0U;
        }
    } else {
        s_app.segment_candidate = candidate;
        s_app.segment_candidate_count = 1U;
    }
}

/*
 * 连续弯方向切换检测：维护最近 N 帧的 position 符号环形缓冲，
 * 统计方向和方向的切换次数。
 */
static void update_curve_direction_history(void)
{
    int8_t current_sign = 0;
    int8_t prev_sign;
    uint8_t window = g_lf_config.seg_curve_direction_window;

    /* 保护：窗口至少为 4，防止除零和数组溢出 */
    if (window < 4U) {
        window = 4U;
    }

    if (s_app.last_frame.position < 0) {
        current_sign = -1;
    } else if (s_app.last_frame.position > 0) {
        current_sign = 1;
    }

    /* 环形缓冲写入 */
    if (s_app.sign_history_index >= window) {
        s_app.sign_history_index = 0U;
    }
    prev_sign = s_app.position_sign_history[s_app.sign_history_index];
    s_app.position_sign_history[s_app.sign_history_index] = current_sign;

    /* 检查是否发生了方向切换（当前帧方向 vs 上一帧方向） */
    if (current_sign != 0 && prev_sign != 0 && current_sign != prev_sign) {
        if (s_app.direction_switch_count < UINT8_MAX) {
            s_app.direction_switch_count += 1U;
        }
    }

    s_app.sign_history_index += 1U;
    if (s_app.sign_history_index >= window) {
        s_app.sign_history_index = 0U;
    }

    /* 衰减旧切换：每 window/2 帧衰减一次切换计数，保持时效性 */
    if ((s_app.sign_history_index % (window / 2U)) == 0U) {
        if (s_app.direction_switch_count > 0U) {
            s_app.direction_switch_count -= 1U;
        }
    }
}

static bool is_continuous_curve(void)
{
    return s_app.segment_type == LF_SEGMENT_TIGHT_CURVE &&
           s_app.direction_switch_count >= g_lf_config.seg_curve_direction_switch_min;
}

static bool frame_is_normal_center_line(const LF_SensorFrame *frame)
{
    uint8_t release_active = g_lf_config.start_straight_guard_active_count;
    if (release_active == 0U) {
        release_active = 4U;
    }

    return frame != NULL && frame->line_detected &&
           frame->active_count <= release_active &&
           abs_i32(frame->position) <= g_lf_config.start_straight_guard_release_error;
}

static bool frame_is_interference(const LF_SensorFrame *frame)
{
    int32_t position_jump;
    bool trusted_was_straight;

    if (!g_lf_config.line_stability_enable || frame == NULL || !frame->line_detected ||
        !s_app.trusted_line_valid) {
        return false;
    }
    if (frame->active_count < g_lf_config.interference_active_count_threshold) {
        return false;
    }
    if (detect_outer_gap_right_angle_side(frame) != 0) {
        return false;
    }

    trusted_was_straight = abs_i32(s_app.last_trusted_position) <=
                           g_lf_config.straight_noise_max_position_error;
    if (frame->edge_hint != 0 && abs_i32(frame->position) >= g_lf_config.curve_prepare_error_threshold &&
        !trusted_was_straight) {
        return false;
    }

    position_jump = abs_i32(frame->position - s_app.last_trusted_position);
    return position_jump >= g_lf_config.interference_position_jump_threshold;
}

static bool sensor_lane_active(const LF_SensorFrame *frame, uint8_t index)
{
    return frame != NULL && index < LF_SENSOR_COUNT &&
           frame->filtered_u16[index] >= g_lf_config.line_detect_min_peak;
}

static bool sensor_lane_active_now(const LF_SensorFrame *frame, uint8_t index)
{
    return frame != NULL && index < LF_SENSOR_COUNT &&
           frame->instant_norm[index] >= g_lf_config.line_detect_min_peak;
}

static bool sensor_lane_dark_now(const LF_SensorFrame *frame, uint8_t index)
{
    return frame != NULL && index < LF_SENSOR_COUNT &&
           frame->instant_norm[index] < g_lf_config.line_detect_min_peak;
}

static uint8_t count_active_range(const LF_SensorFrame *frame, uint8_t first, uint8_t last)
{
    uint8_t count = 0U;
    uint8_t i;

    if (frame == NULL || first >= LF_SENSOR_COUNT) {
        return 0U;
    }
    if (last >= LF_SENSOR_COUNT) {
        last = (uint8_t)(LF_SENSOR_COUNT - 1U);
    }

    for (i = first; i <= last; ++i) {
        if (sensor_lane_active(frame, i)) {
            count++;
        }
    }
    return count;
}

static bool middle_lane_is_good(const LF_SensorFrame *frame)
{
    uint8_t middle_count = count_active_range(frame, 1U, 6U);
    return middle_count == 4U || middle_count == 5U;
}

static int8_t detect_edge_realign_side(const LF_SensorFrame *frame)
{
    uint8_t left_edge_count;
    uint8_t right_edge_count;

    if (!g_lf_config.edge_realign_enable || frame == NULL || !frame->line_detected ||
        middle_lane_is_good(frame)) {
        return 0;
    }

    left_edge_count = count_active_range(frame, 0U, 2U);
    right_edge_count = count_active_range(frame, 5U, 7U);

    if (left_edge_count >= 3U && right_edge_count <= 1U) {
        return -1;
    }
    if (right_edge_count >= 3U && left_edge_count <= 1U) {
        return +1;
    }
    return 0;
}

static int8_t detect_side_wide_curve_arc_side(const LF_SensorFrame *frame)
{
    uint8_t left_side_count;
    uint8_t right_side_count;

    if (frame == NULL || !frame->line_detected) {
        return 0;
    }

    left_side_count = count_active_range(frame, 0U, 4U);
    right_side_count = count_active_range(frame, 3U, 7U);

    if (sensor_lane_active(frame, 0U) && left_side_count >= 5U &&
        left_side_count >= (uint8_t)(right_side_count + 2U)) {
        return -1;
    }
    if (sensor_lane_active(frame, 7U) && right_side_count >= 5U &&
        right_side_count >= (uint8_t)(left_side_count + 2U)) {
        return +1;
    }
    return 0;
}

static int8_t detect_curve_arc_side(const LF_SensorFrame *frame)
{
    uint8_t left_mid_count;
    uint8_t right_mid_count;
    int8_t side_wide;

    if (!g_lf_config.curve_arc_enable || frame == NULL || !frame->line_detected) {
        return 0;
    }

    side_wide = detect_side_wide_curve_arc_side(frame);
    if (side_wide != 0) {
        return side_wide;
    }

    left_mid_count = count_active_range(frame, 1U, 3U);
    right_mid_count = count_active_range(frame, 4U, 6U);

    if (middle_lane_is_good(frame) || detect_edge_realign_side(frame) != 0) {
        return 0;
    }

    if (left_mid_count >= (uint8_t)(right_mid_count + 2U) && left_mid_count >= 2U) {
        return -1;
    }
    if (right_mid_count >= (uint8_t)(left_mid_count + 2U) && right_mid_count >= 2U) {
        return +1;
    }
    return 0;
}

static int8_t detect_outer_gap_right_angle_side(const LF_SensorFrame *frame)
{
    if (frame == NULL || !frame->line_detected) {
        return 0;
    }

    /*
     * 直角入口按当前采样 instant_norm[] 判定，而不是 norm[]/filtered_u16[]。
     * 根因：3点中值和 IIR 滤波都会让刚灭的最外侧通道继续残留为"亮"，导致实车看到 7/8 灭而软件不触发。
     *
     * 用户实测左转直角：1~6 全亮，8 灭，7 可亮可灭。
     * 镜像右转直角：3~8 全亮，1 灭，2 可亮可灭。
     */
    if (sensor_lane_dark_now(frame, 7U) &&
        sensor_lane_active_now(frame, 0U) && sensor_lane_active_now(frame, 1U) &&
        sensor_lane_active_now(frame, 2U) && sensor_lane_active_now(frame, 3U) &&
        sensor_lane_active_now(frame, 4U) && sensor_lane_active_now(frame, 5U)) {
        return -1;
    }
    if (sensor_lane_dark_now(frame, 0U) &&
        sensor_lane_active_now(frame, 2U) && sensor_lane_active_now(frame, 3U) &&
        sensor_lane_active_now(frame, 4U) && sensor_lane_active_now(frame, 5U) &&
        sensor_lane_active_now(frame, 6U) && sensor_lane_active_now(frame, 7U)) {
        return +1;
    }
    return 0;
}

/*
 * 直角转弯检测：传感器模式为"大面积亮 + 暗角集中在一侧"。
 *
 * 三级条件：
 * A. ≥3暗+对侧全亮 → 高置信度直角弯（深入拐角）
 * B. ≥2暗+对侧全亮+≥6active → 中置信度
 * C. ≥1暗+对侧全亮+≥6active+position跳变≥500 → 低置信度但有位置验证
 *    直角弯的1暗伴随大位置跳变（从居中突然到700+），S弯偏移是渐进的。
 *
 * 仅靠单帧传感器模式无法区分"S弯/U弯偏移"和"直角弯入口"，
 * C 级条件通过 position 跳变来区分：直角弯=跳变，S弯=渐进。
 * 时序判别由调用方（arbitrate_running_action）的 consistently_drifting 负责。
 */
static int8_t detect_right_angle_turn_side(const LF_SensorFrame *frame)
{
    uint8_t left_active;
    uint8_t right_active;
    uint8_t inactive_left;
    uint8_t inactive_right;
    uint8_t total_active;
    int8_t outer_gap_side;

    if (frame == NULL || !frame->line_detected || frame->active_count < 4U) {
        return 0;
    }

    outer_gap_side = detect_outer_gap_right_angle_side(frame);
    if (outer_gap_side != 0) {
        return outer_gap_side;
    }

    left_active = count_active_range(frame, 0U, 3U);
    right_active = count_active_range(frame, 4U, 7U);
    inactive_left = 4U - left_active;
    inactive_right = 4U - right_active;
    total_active = frame->active_count;

    /* A级：≥3 暗 + 对侧全亮 → 高置信度 */
    if (inactive_right >= 3U && inactive_left == 0U) return +1;
    if (inactive_left >= 3U && inactive_right == 0U) return -1;

    /* B级：≥2 暗 + 对侧全亮 + ≥6 active */
    if (inactive_right >= 2U && inactive_left == 0U && total_active >= 6U) return +1;
    if (inactive_left >= 2U && inactive_right == 0U && total_active >= 6U) return -1;

    /* C级：≥1 暗 + 对侧全亮 + ≥6 active + position 大跳变 ≥300
     * 直角弯入口：position 从居中突然跳到 300+，单帧跳变 300+
     * S弯/U弯：position 渐进偏移，单帧跳变通常 <200
     * 阈值从500降到300：覆盖单暗传感器场景（|position|≈300） */
    if (total_active >= 6U && s_app.trusted_line_valid) {
        int32_t pos_jump = abs_i32(frame->position - s_app.last_trusted_position);
        if (inactive_right == 1U && inactive_left == 0U && pos_jump >= 300) return +1;
        if (inactive_left == 1U && inactive_right == 0U && pos_jump >= 300) return -1;
    }

    return 0;
}

/*
 * 位置一致性偏移检测：检查最近N帧的position是否持续同向偏移。
 * 用于区分"S弯/U弯中的持续偏移"和"直角弯入口的快速到达"。
 * 连续弯/S弯：position持续同向增长 → 返回 true。
 * 直角弯入口：position快速到达后拐角 → 返回 false。
 */
static bool position_is_consistently_drifting(int8_t check_sign, uint8_t min_frames)
{
    uint8_t window = g_lf_config.seg_curve_direction_window;
    uint8_t count = 0U;
    uint8_t i;

    if (window < 4U) window = 4U;
    if (check_sign == 0 || min_frames == 0U) return false;

    /* sign_history 已在 update_curve_direction_history 中更新（在 arbitrate 之前调用）。
     * sign_history_index 指向下一个写入位置，最新条目在 (index-1+window)%window。
     * 从最新条目往回检查连续同向帧数。 */
    for (i = 0U; i < window && count < min_frames; ++i) {
        uint8_t idx = (uint8_t)((s_app.sign_history_index + window - 1U - i) % window);
        if (s_app.position_sign_history[idx] == check_sign) {
            count++;
        } else {
            break;  /* 遇到不同符号就停止，只算连续的 */
        }
    }
    return count >= min_frames;
}

static bool frame_is_straight_noise(const LF_SensorFrame *frame)
{
    int32_t position_delta;

    if (!g_lf_config.straight_noise_reject_enable || frame == NULL || !frame->line_detected ||
        !s_app.trusted_line_valid) {
        return false;
    }
    if (frame->active_count < g_lf_config.straight_noise_active_count_threshold ||
        frame->signal_sum > g_lf_config.straight_noise_max_sum ||
        abs_i32(frame->position) > g_lf_config.straight_noise_max_position_error) {
        return false;
    }
    position_delta = abs_i32(frame->position - s_app.last_trusted_position);
    return position_delta <= g_lf_config.straight_noise_max_position_delta;
}

static bool frame_is_lead_event(const LF_SensorFrame *frame)
{
    int32_t abs_position;

    if (!g_lf_config.lead_compensation_enable || frame == NULL || !frame->line_detected ||
        frame_is_straight_noise(frame)) {
        return false;
    }
    if (frame->active_count < g_lf_config.lead_event_active_count_threshold ||
        frame->signal_sum < g_lf_config.lead_event_min_sum) {
        return false;
    }

    abs_position = abs_i32(frame->position);
    if (abs_position <= g_lf_config.lead_event_center_error_threshold) {
        return frame->edge_hint == 0 && s_app.lead_entry_memory_count > 0U;
    }
    return abs_position >= g_lf_config.lead_event_entry_error_threshold ||
           frame->edge_hint != 0;
}

static void reset_lead_phase(void)
{
    s_app.lead_phase = (uint8_t)LF_LEAD_PHASE_IDLE;
    s_app.lead_event_count = 0U;
    s_app.lead_phase_ticks = 0U;
    s_app.lead_entry_memory_count = 0U;
    s_app.lead_entry_dir = 0;
}

static void start_lead_advance(void)
{
    s_app.lead_phase = (uint8_t)LF_LEAD_PHASE_ADVANCE;
    s_app.lead_phase_ticks = 0U;
    s_app.lead_event_count = 0U;
}

static int8_t choose_lead_turn_dir(void)
{
    if (s_app.lead_entry_memory_count > 0U && s_app.lead_entry_dir != 0) {
        return s_app.lead_entry_dir;
    }
    return 0;
}

static void update_lead_entry_memory(const LF_SensorFrame *frame, bool interference)
{
    if (frame == NULL || !frame->line_detected || interference) {
        return;
    }

    if (frame->edge_hint != 0) {
        s_app.lead_entry_dir = frame->edge_hint;
        s_app.lead_entry_memory_count = g_lf_config.lead_entry_memory_ticks;
    } else if (abs_i32(frame->position) >= g_lf_config.lead_event_entry_error_threshold) {
        s_app.lead_entry_dir = (frame->position < 0) ? -1 : +1;
        s_app.lead_entry_memory_count = g_lf_config.lead_entry_memory_ticks;
    } else if (s_app.lead_entry_memory_count > 0U) {
        s_app.lead_entry_memory_count--;
    }
}

static void process_lead_phase(void)
{
    int16_t forward;
    int16_t delta;
    int8_t dir;

    if (s_app.lead_phase == (uint8_t)LF_LEAD_PHASE_ADVANCE) {
        forward = limit_degraded_speed(g_lf_config.lead_advance_speed);
        LF_Chassis_SetCommand(forward, forward);
        bump_counter(&s_app.lead_phase_ticks);
        if (s_app.lead_phase_ticks >= g_lf_config.lead_advance_ticks) {
            if (choose_lead_turn_dir() != 0) {
                s_app.lead_phase = (uint8_t)LF_LEAD_PHASE_TURN_HOLD;
                s_app.lead_phase_ticks = 0U;
            } else {
                reset_lead_phase();
                LF_Control_ResetPid(&s_app.pid);
            }
        }
        return;
    }

    if (s_app.lead_phase != (uint8_t)LF_LEAD_PHASE_TURN_HOLD) {
        reset_lead_phase();
        return;
    }

    dir = choose_lead_turn_dir();
    if (dir == 0) {
        reset_lead_phase();
        LF_Control_ResetPid(&s_app.pid);
        return;
    }

    forward = limit_degraded_speed(g_lf_config.lead_turn_speed);
    delta = limit_degraded_speed(g_lf_config.lead_turn_delta);
    if (dir < 0) {
        LF_Chassis_SetCommand((int16_t)(forward - delta), (int16_t)(forward + delta));
    } else {
        LF_Chassis_SetCommand((int16_t)(forward + delta), (int16_t)(forward - delta));
    }

    bump_counter(&s_app.lead_phase_ticks);
    if (s_app.lead_phase_ticks >= g_lf_config.lead_turn_hold_ticks) {
        reset_lead_phase();
        LF_Control_ResetPid(&s_app.pid);
    }
}

static void update_running_window(const LF_SensorFrame *frame, bool interference)
{
    int32_t abs_position;
    int32_t position_delta;
    bool straight_frame;
    bool curve_frame;

    if (frame == NULL || !frame->line_detected) {
        s_app.straight_stable_count = 0U;
        s_app.curve_prepare_count = 0U;
        s_app.straight_noise_count = 0U;
        return;
    }

    if (interference) {
        bump_counter(&s_app.interference_count);
        s_app.straight_stable_count = 0U;
        s_app.straight_noise_count = 0U;
        return;
    }

    s_app.interference_count = 0U;
    abs_position = abs_i32(frame->position);
    position_delta = s_app.trusted_line_valid ? abs_i32(frame->position - s_app.last_trusted_position) : 0;

    if (frame_is_straight_noise(frame)) {
        bump_counter(&s_app.straight_noise_count);
        s_app.curve_prepare_count = 0U;
        return;
    }
    s_app.straight_noise_count = 0U;

    straight_frame = g_lf_config.straight_boost_enable &&
                     abs_position <= g_lf_config.straight_error_threshold &&
                     position_delta <= g_lf_config.straight_delta_threshold &&
                     frame->line_confidence >= g_lf_config.straight_confidence_min &&
                     frame->contrast_value >= g_lf_config.line_detect_min_contrast;
    if (straight_frame) {
        bump_counter(&s_app.straight_stable_count);
    } else {
        s_app.straight_stable_count = 0U;
    }

    curve_frame = g_lf_config.curve_prepare_enable &&
                (abs_position >= g_lf_config.curve_prepare_error_threshold ||
                 position_delta >= g_lf_config.curve_prepare_delta_threshold ||
                 (frame->edge_hint != 0 &&
                  (abs_position >= g_lf_config.curve_prepare_error_threshold ||
                   position_delta >= g_lf_config.curve_prepare_delta_threshold)));
		
    if (curve_frame) {
        bump_counter(&s_app.curve_prepare_count);
    } else {
        s_app.curve_prepare_count = 0U;
    }
}

static float shape_control_error(float error)
{
    int16_t deadband = g_lf_config.control_error_deadband;
    int16_t soft_zone = g_lf_config.control_error_soft_zone;
    float sign = 1.0f;
    float abs_error;

    if (deadband < 0) {
        deadband = (int16_t)(-deadband);
    }
    if (soft_zone < deadband) {
        soft_zone = deadband;
    }
    if (deadband == 0 && soft_zone == 0) {
        return error;
    }
    if (error < 0.0f) {
        sign = -1.0f;
        abs_error = -error;
    } else {
        abs_error = error;
    }
    if (abs_error <= (float)deadband) {
        return 0.0f;
    }
    if (soft_zone > deadband && abs_error < (float)soft_zone) {
        float span = (float)(soft_zone - deadband);
        float shaped = (abs_error - (float)deadband) * (abs_error - (float)deadband) / span;
        return sign * shaped;
    }
    return sign * (abs_error - (float)deadband);
}

#if !TDPS_SIMPLE_CONTROL
static int16_t choose_running_speed(const LF_SensorFrame *frame)
{
    int16_t speed = g_lf_config.base_speed;

    if (frame != NULL &&
        ((frame->position > g_lf_config.adaptive_error_threshold) ||
         (frame->position < (int32_t)(-g_lf_config.adaptive_error_threshold)))) {
        speed = g_lf_config.sharp_turn_speed;
    } else if (g_lf_config.straight_noise_reject_enable &&
               s_app.straight_noise_count >= g_lf_config.straight_noise_confirm_ticks) {
        speed = g_lf_config.base_speed;
    } else if (g_lf_config.curve_prepare_enable &&
               s_app.curve_prepare_count >= g_lf_config.curve_prepare_confirm_ticks) {
        speed = g_lf_config.curve_prepare_speed;
    } else if (frame != NULL &&
               (frame->line_confidence < g_lf_config.adaptive_confidence_threshold ||
                frame->contrast_value < g_lf_config.line_detect_min_contrast)) {
        speed = g_lf_config.adaptive_slow_speed;
    } else if (g_lf_config.straight_boost_enable &&
               s_app.straight_stable_count >= g_lf_config.straight_confirm_ticks) {
        speed = g_lf_config.straight_boost_speed;
    }
    if (g_lf_config.obstacle_avoid_enable &&
        s_app.obstacle_state == LF_RADAR_OBSTACLE_WARN &&
        speed > g_lf_config.obstacle_warn_speed) {
        speed = g_lf_config.obstacle_warn_speed;
    }
    speed = limit_degraded_speed(speed);
    s_app.current_target_speed = speed;
    return speed;
}
#endif /* !TDPS_SIMPLE_CONTROL */

static void hold_last_line_direction(void)
{
    int16_t forward = limit_degraded_speed(g_lf_config.line_hold_speed);
    int16_t turn = limit_degraded_speed(g_lf_config.line_hold_turn_speed);
    int8_t dir = g_lf_config.stable_direction_enable ? s_app.trusted_line_dir : s_app.last_seen_dir;

    if (dir < 0) {
        LF_Chassis_SetCommand((int16_t)(forward - turn), (int16_t)(forward + turn));
    } else {
        LF_Chassis_SetCommand((int16_t)(forward + turn), (int16_t)(forward - turn));
    }
}

static bool obstacle_is_clear_enough(void)
{
    return s_app.obstacle_state == LF_RADAR_OBSTACLE_CLEAR ||
           (s_app.obstacle_distance_mm > 0U &&
            s_app.obstacle_distance_mm >= g_lf_config.obstacle_safe_distance_mm);
}

static bool frame_is_confirmed_line(const LF_SensorFrame *frame, uint16_t min_sum, float min_confidence)
{
    return frame != NULL && frame->line_detected &&
           frame->signal_sum >= min_sum &&
           frame->line_confidence >= min_confidence;
}

static void update_trusted_direction(const LF_SensorFrame *frame, bool interference)
{
    int8_t dir = 0;

    if (frame == NULL || !frame->line_detected || interference) {
        return;
    }
    if (g_lf_config.stable_direction_enable &&
        frame->line_confidence < g_lf_config.direction_update_confidence_min) {
        return;
    }

    if (frame->edge_hint != 0) {
        dir = frame->edge_hint;
    } else if (frame->position < 0) {
        dir = -1;
    } else if (frame->position > 0) {
        dir = +1;
    }

    if (dir != 0) {
        s_app.last_seen_dir = dir;
        s_app.trusted_line_dir = dir;
        s_app.last_trusted_position = frame->position;
        s_app.trusted_line_valid = true;
    }
}

static bool wait_start_line_ready(uint32_t now_ms)
{
    uint32_t held_ms;

    if ((uint32_t)(now_ms - s_app.boot_ms) < g_lf_config.start_min_boot_delay_ms) {
        s_app.wait_start_line_since_ms = 0U;
        return false;
    }

    LF_Sensor_ReadFrame(&s_app.last_frame);
    if (!s_app.last_frame.line_detected) {
        s_app.wait_start_line_since_ms = 0U;
        return false;
    }

    if (s_app.wait_start_line_since_ms == 0U) {
        s_app.wait_start_line_since_ms = now_ms;
    }

    held_ms = now_ms - s_app.wait_start_line_since_ms;
    return held_ms >= g_lf_config.start_line_hold_ms;
}

static void start_calibration(uint32_t now_ms)
{
    LF_Sensor_StartCalibration();
    s_app.calib_start_ms = now_ms;
    s_app.wait_start_line_since_ms = 0U;
    set_reason(LF_APP_REASON_CALIBRATION_STARTED);
    set_state(LF_APP_STATE_CALIBRATING);
    LF_Hook_OnCalibrationBegin();
    LF_Platform_DebugPrint("Calibration start\n");
}

static bool confirm_line_frame(uint8_t *counter, uint8_t required_ticks, uint16_t min_sum, float min_confidence)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);
    if (!frame_is_confirmed_line(&s_app.last_frame, min_sum, min_confidence)) {
        if (counter != NULL) {
            *counter = 0U;
        }
        return false;
    }

    if (counter == NULL) {
        return true;
    }
    if (*counter < required_ticks) {
        *counter += 1U;
    }
    return *counter >= required_ticks;
}

static void enter_recovery_phase(LF_RecoverPhase phase, uint32_t now_ms)
{
    s_app.recover_phase = (uint8_t)phase;
    s_app.recover_phase_start_ms = now_ms;
    s_app.recover_confirm_count = 0U;
}

static void complete_line_recovery(LF_AppReason reason)
{
    s_app.recovery_count++;
    LF_RunLog_AddRecovery();
    LF_Control_ResetPid(&s_app.pid);
    set_reason(reason);
    set_state(LF_APP_STATE_RUNNING);
    LF_Hook_OnLineRecovered();
}

static bool fork_elapsed(uint32_t now_ms, uint32_t duration_ms)
{
    return (uint32_t)(now_ms - s_app.fork_state_start_ms) >= duration_ms;
}

static void enter_fork_state(LF_AppState state, uint32_t now_ms)
{
    s_app.fork_state_start_ms = now_ms;
    set_state(state);
}

static bool fork_cooldown_active(uint32_t now_ms)
{
    return g_lf_config.fork_enable &&
           g_lf_config.fork_cooldown_ms > 0U &&
           s_app.fork_decision != 0 &&
           !fork_elapsed(now_ms, g_lf_config.fork_cooldown_ms);
}

static bool frame_looks_like_fork(const LF_SensorFrame *frame)
{
    uint32_t left_sum = 0U;
    uint32_t right_sum = 0U;
    uint8_t active_count = 0U;
    uint32_t i;
    uint16_t threshold;
    int32_t max_abs_position;

    if (frame == NULL || !frame->line_detected || frame_is_straight_noise(frame) ||
        frame->signal_sum < g_lf_config.fork_detect_min_sum) {
        return false;
    }

    max_abs_position = g_lf_config.fork_detect_max_abs_position;
    if (max_abs_position >= 0 &&
        (frame->position > max_abs_position || frame->position < (int32_t)(-max_abs_position))) {
        return false;
    }

    threshold = g_lf_config.fork_sensor_active_threshold;
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        uint16_t value = frame->filtered_u16[i];
        if (value >= threshold) {
            active_count += 1U;
        }
        if (i < (LF_SENSOR_COUNT / 2U)) {
            left_sum += value;
        } else {
            right_sum += value;
        }
    }

    return active_count >= g_lf_config.fork_detect_min_active_sensors &&
           left_sum >= g_lf_config.fork_left_min_sum &&
           right_sum >= g_lf_config.fork_right_min_sum;
}

static bool fork_radar_indicates_left_blocked(void);
static void enter_fork_commit(int8_t decision, uint32_t now_ms, LF_AppReason reason);

static void reset_fork_sample_counters(void)
{
    s_app.fork_block_count = 0U;
    s_app.fork_valid_sample_count = 0U;
    s_app.fork_last_sample_frame_count = s_app.radar_frame_count;
    s_app.fork_decision = 0;
    s_app.fork_min_distance_mm = UINT16_MAX;
    s_app.fork_radar_stale = false;
    s_app.fork_reacquire_count = 0U;
}

static void start_fork_sample(uint32_t now_ms)
{
    int16_t sample_speed = limit_degraded_speed(g_lf_config.fork_sample_speed);

    reset_fork_sample_counters();
    LF_Control_ResetPid(&s_app.pid);
    set_reason(LF_APP_REASON_FORK_DETECTED);
    enter_fork_state(LF_APP_STATE_FORK_SAMPLE, now_ms);

    if (fork_radar_indicates_left_blocked() && s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        s_app.fork_valid_sample_count = 1U;
        s_app.fork_block_count = 1U;
        s_app.fork_min_distance_mm = s_app.obstacle_distance_mm;
        enter_fork_commit(+1, now_ms, LF_APP_REASON_FORK_LEFT_BLOCKED);
        return;
    }

    if (sample_speed == 0) {
        LF_Chassis_Stop();
    } else {
        LF_Chassis_SetCommand(sample_speed, sample_speed);
    }
}

static bool fork_radar_sample_fresh(void)
{
    return s_app.radar_frame_valid &&
           s_app.radar_frame_age_ms <= g_lf_config.fork_radar_max_age_ms;
}

static bool fork_radar_indicates_left_blocked(void)
{
    return fork_radar_sample_fresh() &&
           s_app.radar_has_target &&
           s_app.obstacle_distance_mm >= g_lf_config.fork_radar_min_distance_mm &&
           s_app.obstacle_distance_mm <= g_lf_config.fork_radar_block_distance_mm;
}

static void enter_fork_commit(int8_t decision, uint32_t now_ms, LF_AppReason reason)
{
    s_app.fork_decision = (decision < 0) ? -1 : +1;
    s_app.fork_reacquire_count = 0U;
    LF_RunLog_Record(LF_LOG_FORK, (uint8_t)(s_app.fork_decision + 1), s_app.fork_min_distance_mm);
    LF_Control_ResetPid(&s_app.pid);
    set_reason(reason);
    enter_fork_state(s_app.fork_decision < 0 ? LF_APP_STATE_FORK_COMMIT_LEFT : LF_APP_STATE_FORK_COMMIT_RIGHT,
                     now_ms);
}

static void choose_fork_fallback(uint32_t now_ms)
{
    int8_t fallback = (g_lf_config.fork_fallback_branch > 0) ? +1 : -1;
    s_app.fork_radar_stale = true;
    enter_fork_commit(fallback,
                      now_ms,
                      fallback > 0 ? LF_APP_REASON_FORK_FALLBACK_RIGHT : LF_APP_REASON_FORK_FALLBACK_LEFT);
}

static void process_fork_sample(uint32_t now_ms)
{
    int16_t sample_speed = limit_degraded_speed(g_lf_config.fork_sample_speed);

    if (sample_speed == 0) {
        LF_Chassis_Stop();
    } else {
        LF_Chassis_SetCommand(sample_speed, sample_speed);
    }

    if (s_app.radar_frame_count != s_app.fork_last_sample_frame_count) {
        s_app.fork_last_sample_frame_count = s_app.radar_frame_count;
        if (fork_radar_sample_fresh()) {
            if (s_app.fork_valid_sample_count < UINT8_MAX) {
                s_app.fork_valid_sample_count += 1U;
            }
            if (s_app.obstacle_distance_mm > 0U && s_app.obstacle_distance_mm < s_app.fork_min_distance_mm) {
                s_app.fork_min_distance_mm = s_app.obstacle_distance_mm;
            }
            if (fork_radar_indicates_left_blocked() && s_app.fork_block_count < UINT8_MAX) {
                s_app.fork_block_count += 1U;
            }
        }
    } else if (s_app.fork_valid_sample_count == 0U && fork_radar_sample_fresh()) {
        s_app.fork_last_sample_frame_count = s_app.radar_frame_count;
        s_app.fork_valid_sample_count = 1U;
        if (s_app.obstacle_distance_mm > 0U) {
            s_app.fork_min_distance_mm = s_app.obstacle_distance_mm;
        }
        if (fork_radar_indicates_left_blocked()) {
            s_app.fork_block_count = 1U;
        }
    }

    if (s_app.fork_block_count >= g_lf_config.fork_radar_block_confirm_frames ||
        (fork_radar_indicates_left_blocked() && s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK)) {
        enter_fork_commit(+1, now_ms, LF_APP_REASON_FORK_LEFT_BLOCKED);
        return;
    }

    if (!fork_elapsed(now_ms, g_lf_config.fork_sample_ms)) {
        return;
    }

    if (s_app.fork_valid_sample_count < g_lf_config.fork_radar_valid_min_samples) {
        set_reason(LF_APP_REASON_FORK_RADAR_STALE);
        choose_fork_fallback(now_ms);
        return;
    }

    enter_fork_commit(-1, now_ms, LF_APP_REASON_FORK_LEFT_CLEAR);
}

static void set_fork_commit_command(int8_t decision)
{
    int16_t speed = limit_degraded_speed(g_lf_config.fork_commit_speed);
    int16_t turn = g_lf_config.fork_commit_turn_speed;

    if (decision < 0) {
        LF_Chassis_SetCommand((int16_t)(speed - turn), (int16_t)(speed + turn));
    } else {
        LF_Chassis_SetCommand((int16_t)(speed + turn), (int16_t)(speed - turn));
    }
}

static void process_fork_commit(uint32_t now_ms, int8_t decision)
{
    set_fork_commit_command(decision);

    if (fork_elapsed(now_ms, g_lf_config.fork_commit_min_ms)) {
        LF_Sensor_ReadFrame(&s_app.last_frame);
        if (frame_is_confirmed_line(&s_app.last_frame,
                                    g_lf_config.fork_line_reacquire_min_sum,
                                    g_lf_config.recover_confidence_min)) {
            enter_fork_state(LF_APP_STATE_FORK_REACQUIRE, now_ms);
            return;
        }
    }

    if (fork_elapsed(now_ms, g_lf_config.fork_commit_ms)) {
        enter_fork_state(LF_APP_STATE_FORK_REACQUIRE, now_ms);
    }
}

static void process_fork_reacquire(uint32_t now_ms)
{
    if (confirm_line_frame(&s_app.fork_reacquire_count,
                           g_lf_config.fork_reacquire_confirm_ticks,
                           g_lf_config.fork_line_reacquire_min_sum,
                           g_lf_config.recover_confidence_min)) {
        LF_Control_ResetPid(&s_app.pid);
        s_app.fork_detect_count = 0U;
        set_reason(LF_APP_REASON_FORK_COMPLETED);
        enter_fork_state(LF_APP_STATE_RUNNING, now_ms);
        return;
    }

    if (s_app.fork_decision != 0) {
        set_fork_commit_command(s_app.fork_decision);
    } else {
        LF_Chassis_Stop();
    }

    if (fork_elapsed(now_ms, g_lf_config.fork_reacquire_timeout_ms)) {
        s_app.recover_start_ms = now_ms;
        enter_recovery_phase(LF_RECOVER_SWEEP, now_ms);
        set_reason(LF_APP_REASON_FORK_FAILED);
        enter_fork_state(LF_APP_STATE_RECOVERING, now_ms);
    }
}

static void refresh_radar_snapshot(uint32_t now_ms)
{
    const LF_RadarState *radar;

    LF_Radar_Tick(now_ms);
    radar = LF_Radar_GetState();

    s_app.obstacle_state = radar->obstacle_state;
    s_app.obstacle_distance_mm = radar->distance_mm;
    s_app.radar_parse_error_count = radar->parse_error_count;
    s_app.radar_has_target = radar->has_target;
    s_app.radar_frame_valid = radar->frame_valid;
    s_app.radar_target_state = radar->target_state;
    s_app.radar_frame_type = radar->frame_type;
    s_app.radar_frame_count = radar->frame_count;
    s_app.radar_last_update_ms = radar->last_update_ms;
    s_app.radar_frame_age_ms = radar->frame_valid ? (uint32_t)(now_ms - radar->last_update_ms) : 0U;
}

static void run_calibration_motion(uint32_t now_ms)
{
    uint32_t elapsed;
    uint32_t slot;

    if (g_lf_config.sensor_fast_calibration) {
        return;  /* 快速标定：不旋转，车保持静止 */
    }

    elapsed = now_ms - s_app.calib_start_ms;
    slot = elapsed / g_lf_config.calibration_switch_interval_ms;
    int16_t spin = g_lf_config.calibration_spin_speed;

    /*
     * 标定期间原地左右摆动：
     * 目的不是“跑起来”，而是让传感器视野尽量覆盖黑白区域，提升 min/max 质量。
     */
    if ((slot & 0x1U) == 0U) {
        LF_Chassis_SetCommand(spin, (int16_t)(-spin));
    } else {
        LF_Chassis_SetCommand((int16_t)(-spin), spin);
    }
}

/*
 * 巡线决策仲裁：将原来隐式嵌套的 if-return 链改为显式优先级枚举。
 *
 * 优先级从高到低：
 *   0. LINE_LOST       — 丢线立即进入恢复，最高优先级
 *   1. FORK            — 岔路口采样决策（领先补偿期间抑制）
 *   2. OBSTACLE        — 雷达障碍触发避障
 *   3. LEAD_COMPENSATION — 入弯前馈转向（传感器 22cm 前置量的几何补偿）
 *   4. PID_CONTROL     — 正常巡线，最低优先级
 *
 * 同级内不互抢——领先补偿一旦启动（lead_phase ≠ IDLE），
 * 在完成前不会被低优先级的岔路/障碍覆盖；但丢线仍可中断领先补偿。
 */
typedef enum {
    LF_RUN_ACTION_NONE               = 0,
    LF_RUN_ACTION_LINE_LOST,
    LF_RUN_ACTION_FORK,
    LF_RUN_ACTION_OBSTACLE,
    LF_RUN_ACTION_LEAD_COMPENSATION,
    LF_RUN_ACTION_EDGE_REALIGN,
    LF_RUN_ACTION_CURVE_ARC,
    LF_RUN_ACTION_REORIENT,
    LF_RUN_ACTION_PID_CONTROL,
} LF_RunAction;

typedef struct {
    LF_RunAction action;
    bool fork_candidate;
    bool interference;
    bool start_guard_active;
} LF_RunDecision;

static void start_reorient(uint32_t now_ms); /* 前向声明，定义在第 ~1313 行 */

static void handle_line_lost(uint32_t now_ms)
{
    int16_t forward;
    int16_t turn;
    int8_t dir;
    uint8_t effective_grace;

    reset_lead_phase();
    s_app.interference_hold_count = 0U;
    s_app.straight_noise_count = 0U;
    s_app.fork_detect_count = 0U;

    if (!g_lf_config.stable_direction_enable && s_app.last_frame.edge_hint != 0) {
        s_app.last_seen_dir = s_app.last_frame.edge_hint;
    }
    if (s_app.line_lost_count < UINT8_MAX) {
        s_app.line_lost_count += 1U;
    }

    /* 连续弯中扩展宽容期：减少不必要的 recovery */
    effective_grace = g_lf_config.line_lost_grace_ticks;
    if (g_lf_config.segment_control_enable && is_continuous_curve()) {
        effective_grace = (uint8_t)(effective_grace + g_lf_config.seg_curve_grace_ticks_extra);
    }

    if (s_app.line_lost_count <= effective_grace) {
        /* 连续弯中的 grace 保持：使用更激进的转向，降速 + 大差速 */
        if (g_lf_config.segment_control_enable && is_continuous_curve()) {
            dir = g_lf_config.stable_direction_enable ? s_app.trusted_line_dir : s_app.last_seen_dir;
            forward = limit_degraded_speed(60);  /* 更低速度，留更多修正余量 */
            /* 使用 line_hold_turn_speed 的 1.5 倍作为转向力度 */
            turn = limit_degraded_speed((int16_t)(g_lf_config.line_hold_turn_speed * 3 / 2));
            if (turn < 80) turn = 80;  /* 最低转向力度 */
            if (dir < 0) {
                LF_Chassis_SetCommand((int16_t)(forward - turn), (int16_t)(forward + turn));
            } else {
                LF_Chassis_SetCommand((int16_t)(forward + turn), (int16_t)(forward - turn));
            }
        } else {
            hold_last_line_direction();
        }
        set_reason(LF_APP_REASON_LINE_LOST);
        return;
    }

    /* grace 用尽后，连续弯中急转向尝试（3帧） */
    if (g_lf_config.segment_control_enable && is_continuous_curve() &&
        s_app.line_lost_count <= (uint8_t)(effective_grace + 3U)) {
        int8_t dir_sign = (s_app.trusted_line_dir != 0) ? s_app.trusted_line_dir : s_app.last_seen_dir;
        int16_t emergency_turn = limit_degraded_speed((int16_t)(g_lf_config.line_hold_turn_speed * 2));
        if (emergency_turn < 100) emergency_turn = 100;
        if (dir_sign < 0) {
            LF_Chassis_SetCommand((int16_t)(-emergency_turn), emergency_turn);
        } else {
            LF_Chassis_SetCommand(emergency_turn, (int16_t)(-emergency_turn));
        }
        set_reason(LF_APP_REASON_LINE_LOST);
        return;
    }

    /* 丢线前刚检测到急弯且位置偏差大 → 原地旋转对准（替代 recovery）
     * 冷却期检查：防止 U 弯中反复触发。
     * 仅靠 position 门限触发，不使用 active_count（太敏感）。 */
    {
        bool cooling = g_lf_config.reorient_cooldown_ms > 0U &&
            s_app.reorient_last_finish_ms > 0U &&
            (uint32_t)(now_ms - s_app.reorient_last_finish_ms) < g_lf_config.reorient_cooldown_ms;
        if (g_lf_config.reorient_enable && !cooling &&
            s_app.last_strong_curve_side != 0 &&
            abs_i32(s_app.last_curve_position) >= g_lf_config.reorient_position_threshold) {
            s_app.reorient_spin_dir = s_app.last_strong_curve_side;
            s_app.last_strong_curve_side = 0;
            start_reorient(now_ms);
            return;
        }
    }

    s_app.recover_start_ms = now_ms;
    enter_recovery_phase(LF_RECOVER_BACKTRACK, s_app.recover_start_ms);
    set_reason(LF_APP_REASON_LINE_LOST);
    set_state(LF_APP_STATE_RECOVERING);
    LF_Hook_OnLineLost();
}

static void process_edge_realign(void)
{
    int16_t speed = limit_degraded_speed(g_lf_config.edge_realign_speed);
    int16_t delta = limit_degraded_speed(g_lf_config.edge_realign_delta);
    int8_t sign = (g_lf_config.edge_realign_dir_sign < 0) ? -1 : +1;
    int8_t steering_sign = (g_lf_config.steering_dir_sign < 0) ? -1 : +1;
    int16_t correction;

    reset_lead_phase();
    LF_Control_ResetPid(&s_app.pid);

    correction = (int16_t)((int16_t)s_app.edge_realign_side * (int16_t)sign *
                           (int16_t)steering_sign * delta);
    LF_Chassis_SetCommand((int16_t)(speed + correction), (int16_t)(speed - correction));
    s_app.current_target_speed = speed;
}

static void process_curve_arc(void)
{
    int16_t speed = limit_degraded_speed(g_lf_config.curve_arc_speed);
    float error;
    int16_t correction;
    int16_t left_cmd;
    int16_t right_cmd;
    int16_t saved_max_motor_delta = g_lf_config.max_motor_delta;
    bool override_motor_delta = g_lf_config.curve_arc_max_motor_delta > 0;

    reset_lead_phase();

    error = (float)s_app.last_frame.position;
    /* 弯中用完整 PID（含积分项）：纯 P 在恒定曲率路径上是鞍点平衡，
     * D 项衰减后车会持续偏离弯道。小积分消除弯道跟踪的稳态误差。
     * 死区设 1（近零）保持弯中灵敏响应。积分保护链（分离/反饱和）防止过冲。 */
    correction = LF_Control_UpdatePid(error, 0.01f, &s_app.pid);
    /* PID 内部已按 steering_dir_sign 约定输出，直接传给 ComputeMotorCmd。 */

    LF_Control_ComputeMotorCmd(speed, correction, &left_cmd, &right_cmd);
    left_cmd = limit_degraded_speed(left_cmd);
    right_cmd = limit_degraded_speed(right_cmd);
    if (override_motor_delta) {
        g_lf_config.max_motor_delta = g_lf_config.curve_arc_max_motor_delta;
    }
    LF_Chassis_SetCommand(left_cmd, right_cmd);
    if (override_motor_delta) {
        g_lf_config.max_motor_delta = saved_max_motor_delta;
    }
    s_app.current_target_speed = speed;
}

/*
 * 每控制周期调用一次，按优先级选择唯一动作。
 * 返回前已完成本周期所有窗口计数器更新。
 */
static LF_RunDecision arbitrate_running_action(uint32_t now_ms)
{
    LF_RunDecision d;
    d.fork_candidate = false;
    d.interference = frame_is_interference(&s_app.last_frame);
    if (d.interference) {
        uint16_t hold_ticks = g_lf_config.interference_hold_ticks;
        if (hold_ticks == 0U) {
            hold_ticks = 1U;
        }
        s_app.interference_hold_count = hold_ticks;
    } else if (s_app.interference_hold_count > 0U && frame_is_normal_center_line(&s_app.last_frame)) {
        s_app.interference_hold_count = 0U;
    } else if (s_app.interference_hold_count > 0U) {
        s_app.interference_hold_count--;
        d.interference = true;
    }
    d.start_guard_active = false;

    /* 优先级 0：丢线（不可降级，立即接管） */
    if (!s_app.last_frame.line_detected) {
        d.action = LF_RUN_ACTION_LINE_LOST;
        return d;
    }

    s_app.line_lost_count = 0U;
    update_running_window(&s_app.last_frame, d.interference);
    d.start_guard_active = start_straight_guard_tick(now_ms, &s_app.last_frame);

    /* 保存急弯检测结果，供丢线时判断是否原地旋转对准。
     * side_wide 需要 5+ 传感器活跃（严格），curve_arc 只需 2+ 差异（宽松）。
     * 两者都记录：side_wide 用于在线时触发，curve_arc 用于丢线时的紧急判断。
     * 注意：curve_arc 仅在 position>=1000 时才保存——避免正常偏线时误设置。 */
    if (!d.interference) {
        int8_t sw = detect_side_wide_curve_arc_side(&s_app.last_frame);
        int8_t cs = detect_curve_arc_side(&s_app.last_frame);
        if (sw != 0) {
            s_app.last_strong_curve_side = sw;
            s_app.last_curve_position = s_app.last_frame.position;
        } else if (cs != 0 && abs_i32(s_app.last_frame.position) >= 1000) {
            s_app.last_strong_curve_side = cs;
            s_app.last_curve_position = s_app.last_frame.position;
        }
    }

    /* 优先级 1：岔路（需连续确认 fork_detect_confirm_ticks 帧） */
    if (!d.start_guard_active && !d.interference && g_lf_config.fork_enable &&
        !fork_cooldown_active(now_ms) &&
        frame_looks_like_fork(&s_app.last_frame)) {
        d.fork_candidate = true;
        if (s_app.fork_detect_count < UINT8_MAX) {
            s_app.fork_detect_count += 1U;
        }
        if (s_app.fork_detect_count >= g_lf_config.fork_detect_confirm_ticks) {
            d.action = LF_RUN_ACTION_FORK;
            return d;
        }
    } else {
        s_app.fork_detect_count = 0U;
    }

    /* 优先级 2：雷达障碍 */
    if (!d.fork_candidate && g_lf_config.obstacle_avoid_enable &&
        s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        d.action = LF_RUN_ACTION_OBSTACLE;
        return d;
    }

    /* 明确直角灯型优先于箭头/宽黑干扰保护，避免 1~6亮、7/8灭 被干扰逻辑吃掉。 */
    {
        int8_t outer_right_angle_side = detect_outer_gap_right_angle_side(&s_app.last_frame);
        bool reorient_cooling = g_lf_config.reorient_cooldown_ms > 0U &&
            s_app.reorient_last_finish_ms > 0U &&
            (uint32_t)(now_ms - s_app.reorient_last_finish_ms) < g_lf_config.reorient_cooldown_ms;
        if (g_lf_config.reorient_enable && !reorient_cooling && outer_right_angle_side != 0) {
            s_app.reorient_spin_dir = outer_right_angle_side;
            d.action = LF_RUN_ACTION_REORIENT;
            return d;
        }
    }

    /* 箭头/宽黑干扰：不让干扰帧触发前探/弯道/岔路，但仍交给正常 PID 用上一可信位置纠偏。 */
    if (d.interference) {
        d.action = LF_RUN_ACTION_PID_CONTROL;
        return d;
    }

    /* 优先级 3：领先补偿——传感器 22cm 前置导致的位置超前响应 */
    {
        int8_t edge_side = detect_edge_realign_side(&s_app.last_frame);
        uint8_t confirm_ticks = g_lf_config.edge_realign_confirm_ticks;
        if (confirm_ticks == 0U) {
            confirm_ticks = 1U;
        }
        if (edge_side != 0 && detect_curve_arc_side(&s_app.last_frame) == 0) {
            if (s_app.edge_realign_side == edge_side) {
                bump_counter(&s_app.edge_realign_count);
            } else {
                s_app.edge_realign_side = edge_side;
                s_app.edge_realign_count = 1U;
            }
            if (s_app.edge_realign_count >= confirm_ticks) {
                d.action = LF_RUN_ACTION_EDGE_REALIGN;
                return d;
            }
        } else {
            s_app.edge_realign_count = 0U;
            s_app.edge_realign_side = 0;
        }
    }

    {
        int8_t strong_curve_side = detect_side_wide_curve_arc_side(&s_app.last_frame);
        int8_t right_angle_side = detect_right_angle_turn_side(&s_app.last_frame);
        bool lead_active = (s_app.lead_phase != (uint8_t)LF_LEAD_PHASE_IDLE) ||
                           frame_is_lead_event(&s_app.last_frame);
        bool curve_allowed = (!lead_active && (!d.start_guard_active || strong_curve_side != 0));

        /* 急弯原地旋转对准。
         *
         * 核心难点：单帧传感器模式无法区分"S弯/U弯中的偏移"和"直角弯入口"。
         * 两者可能产生完全相同的传感器分布（如 1,1,1,1,1,1,0,0）。
         *
         * 解法：加入时序判别——
         * - S弯/U弯：position 持续同向偏移多帧（车一直在跟着弯走）
         * - 直角弯：position 快速到达后拐角（车到达拐角点，position 跳变但不持续增长）
         * - position_is_consistently_drifting 检查最近4帧是否持续同向。
         *
         * 冷却期检查：防止 U 弯中反复触发 REORIENT 导致振荡。 */
        {
            bool reorient_cooling = g_lf_config.reorient_cooldown_ms > 0U &&
                s_app.reorient_last_finish_ms > 0U &&
                (uint32_t)(now_ms - s_app.reorient_last_finish_ms) < g_lf_config.reorient_cooldown_ms;

            {
                uint8_t right_angle_confirm_ticks = g_lf_config.right_angle_confirm_ticks;
                if (right_angle_confirm_ticks == 0U) {
                    right_angle_confirm_ticks = 1U;
                }

                if (g_lf_config.reorient_enable && !reorient_cooling && right_angle_side != 0) {
                    bool right_angle_outer_gap = detect_outer_gap_right_angle_side(&s_app.last_frame) != 0;
                    if (right_angle_outer_gap ||
                        abs_i32(s_app.last_frame.position) >= g_lf_config.reorient_position_threshold) {
                        int8_t pos_sign = (s_app.last_frame.position > 0) ? 1 : -1;
                        if (right_angle_outer_gap || !position_is_consistently_drifting(pos_sign, 4U)) {
                            if (s_app.right_angle_side == right_angle_side) {
                                bump_counter(&s_app.right_angle_detect_count);
                            } else {
                                s_app.right_angle_side = right_angle_side;
                                s_app.right_angle_detect_count = 1U;
                            }
                            if (s_app.right_angle_detect_count >= right_angle_confirm_ticks) {
                                s_app.reorient_spin_dir = right_angle_side;
                                d.action = LF_RUN_ACTION_REORIENT;
                                return d;
                            }
                        } else {
                            s_app.right_angle_detect_count = 0U;
                            s_app.right_angle_side = 0;
                        }
                    } else {
                        s_app.right_angle_detect_count = 0U;
                        s_app.right_angle_side = 0;
                    }
                } else {
                    s_app.right_angle_detect_count = 0U;
                    s_app.right_angle_side = 0;
                }
            }
            /* 带直角特征的宽线先经过 right_angle_confirm_ticks 确认。 */
            if (g_lf_config.reorient_enable && !reorient_cooling && strong_curve_side != 0 &&
                right_angle_side == 0 &&
                abs_i32(s_app.last_frame.position) >= g_lf_config.reorient_position_threshold) {
                int8_t pos_sign = (s_app.last_frame.position > 0) ? 1 : -1;
                if (!position_is_consistently_drifting(pos_sign, 4U)) {
                    s_app.reorient_spin_dir = strong_curve_side;
                    d.action = LF_RUN_ACTION_REORIENT;
                    return d;
                }
            }
        }
        if (curve_allowed) {
            int8_t curve_side = detect_curve_arc_side(&s_app.last_frame);
            uint8_t confirm_ticks = g_lf_config.curve_arc_confirm_ticks;
            uint8_t release_ticks = g_lf_config.curve_arc_release_ticks;
            if (confirm_ticks == 0U) {
                confirm_ticks = 1U;
            }
            if (release_ticks == 0U) {
                release_ticks = 1U;
            }

            if (curve_side != 0) {
                s_app.curve_arc_release_count = 0U;
                if (s_app.curve_arc_side == curve_side) {
                    bump_counter(&s_app.curve_arc_count);
                } else {
                    s_app.curve_arc_side = curve_side;
                    s_app.curve_arc_count = 1U;
                }
            } else if (middle_lane_is_good(&s_app.last_frame)) {
                bump_counter(&s_app.curve_arc_release_count);
                if (s_app.curve_arc_release_count >= release_ticks) {
                    s_app.curve_arc_count = 0U;
                    s_app.curve_arc_release_count = 0U;
                    s_app.curve_arc_side = 0;
                }
            } else {
                s_app.curve_arc_count = 0U;
                s_app.curve_arc_release_count = 0U;
                s_app.curve_arc_side = 0;
            }

            if (s_app.curve_arc_side != 0 && s_app.curve_arc_count >= confirm_ticks) {
                d.action = LF_RUN_ACTION_CURVE_ARC;
                return d;
            }
        } else {
            s_app.curve_arc_count = 0U;
            s_app.curve_arc_release_count = 0U;
            s_app.curve_arc_side = 0;
        }

    }

    if (!d.start_guard_active && s_app.lead_phase != (uint8_t)LF_LEAD_PHASE_IDLE) {
        d.action = LF_RUN_ACTION_LEAD_COMPENSATION;
        return d;
    }

    if (!d.start_guard_active && frame_is_lead_event(&s_app.last_frame)) {
        bump_counter(&s_app.lead_event_count);
        if (s_app.lead_event_count >= g_lf_config.lead_event_confirm_ticks) {
            start_lead_advance();
            d.action = LF_RUN_ACTION_LEAD_COMPENSATION;
            return d;
        }
    } else {
        s_app.lead_event_count = 0U;
    }
    if (!d.start_guard_active) {
        update_lead_entry_memory(&s_app.last_frame, d.interference);
    }

    /* 优先级 4：正常 PID 控制 */
    d.action = LF_RUN_ACTION_PID_CONTROL;
    return d;
}

/*
 * 原地旋转对准：检测到急弯/直角弯时，停车→(可选)前进靠近弯点→原地旋转→中间4路传感器对准线→恢复巡线。
 *
 * REORIENT_STOP: 停车，记录旋转方向和起始时间。
 * REORIENT_APPROACH: (可选)低速前进靠近弯点，让驱动轮到达转弯位置。
 * REORIENT_SPIN: 两轮反向旋转，持续检测中间传感器是否看到线。
 * REORIENT_CONFIRM: 中间传感器对准确认，连续 N 帧稳定后恢复 RUNNING。
 */
static void set_reorient_spin_command(void)
{
    int16_t spin_speed = g_lf_config.reorient_spin_speed;
    int16_t left_cmd;
    int16_t right_cmd;
    int16_t saved_max_motor_delta = g_lf_config.max_motor_delta;

    if (s_app.reorient_spin_dir > 0) {
        left_cmd = spin_speed;
        right_cmd = (int16_t)(-spin_speed);
    } else {
        left_cmd = (int16_t)(-spin_speed);
        right_cmd = spin_speed;
    }

    g_lf_config.max_motor_delta = 0;
    LF_Chassis_SetCommand(left_cmd, right_cmd);
    g_lf_config.max_motor_delta = saved_max_motor_delta;
}

static void start_reorient(uint32_t now_ms)
{
    reset_lead_phase();
    LF_Control_ResetPid(&s_app.pid);
    s_app.curve_arc_count = 0U;
    s_app.curve_arc_release_count = 0U;
    s_app.curve_arc_side = 0;
    s_app.reorient_start_ms = now_ms;
    s_app.reorient_confirm_count = 0U;
    s_app.right_angle_detect_count = 0U;
    s_app.right_angle_side = 0;
    s_app.reorient_retry_count = 0U;
    s_app.reorient_retry_reverse = false;
    s_app.reorient_backtrack_active = false;
    s_app.reorient_backtrack_start_ms = 0U;
    set_reason(LF_APP_REASON_REORIENT_STARTED);

    /* 如果是直角弯触发且 position 仍在中间附近，先前进靠近弯点 */
    if (g_lf_config.reorient_approach_ms > 0U &&
        abs_i32(s_app.last_frame.position) < 500) {
        set_state(LF_APP_STATE_REORIENT_APPROACH);
        LF_Platform_DebugPrint("Reorient: approach\n");
    } else if (g_lf_config.reorient_stop_ms == 0U) {
        s_app.reorient_start_ms = now_ms;
        set_reorient_spin_command();
        set_state(LF_APP_STATE_REORIENT_SPIN);
        LF_Platform_DebugPrint("Reorient: spin now\n");
    } else {
        set_state(LF_APP_STATE_REORIENT_STOP);
        LF_Chassis_Stop();
        LF_Platform_DebugPrint("Reorient: stop\n");
    }
}

static bool middle_sensors_aligned(const LF_SensorFrame *frame)
{
    /* 中间 4 路传感器 (2~5) 至少 3 个活跃，说明线在传感器阵列中央 */
    return count_active_range(frame, 2U, 5U) >= 3U;
}

static void process_reorient_approach(uint32_t now_ms)
{
    int16_t approach_speed = limit_degraded_speed(g_lf_config.reorient_approach_speed);

    LF_Sensor_ReadFrame(&s_app.last_frame);

    if (time_reached(now_ms, s_app.reorient_start_ms, g_lf_config.reorient_approach_ms)) {
        /* 前进完成，停车进入旋转 */
        LF_Chassis_Stop();
        set_state(LF_APP_STATE_REORIENT_STOP);
        LF_Platform_DebugPrint("Reorient: approach done, stop\n");
        return;
    }
    /* 低速直行靠近弯点 */
    LF_Chassis_SetCommand(approach_speed, approach_speed);
}

static void reorient_retry_or_stop(uint32_t now_ms)
{
    if (!g_lf_config.reorient_backtrack_enable ||
        s_app.reorient_retry_count >= g_lf_config.reorient_max_retries) {
        LF_Chassis_Stop();
        set_reason(LF_APP_REASON_REORIENT_TIMEOUT);
        set_state(LF_APP_STATE_STOPPED);
        LF_Platform_DebugPrint("Reorient: timeout, max retries exhausted\n");
        return;
    }

    /* 倒车重试 */
    s_app.reorient_retry_count += 1U;
    s_app.reorient_retry_reverse = !s_app.reorient_retry_reverse;
    s_app.reorient_backtrack_active = true;
    s_app.reorient_backtrack_start_ms = now_ms;

    /* 反转旋转方向 */
    s_app.reorient_spin_dir = (int8_t)(-s_app.reorient_spin_dir);

    {
        char dbg_buf[64];
        snprintf(dbg_buf, sizeof(dbg_buf),
                 "Reorient: backtrack retry %d\n",
                 (int)s_app.reorient_retry_count);
        LF_Platform_DebugPrint(dbg_buf);
    }
}

static void process_reorient_backtrack(uint32_t now_ms)
{
    int16_t backtrack_speed = g_lf_config.reorient_backtrack_speed;
    uint32_t backtrack_ms = g_lf_config.reorient_backtrack_ms;
    if (s_app.reorient_retry_reverse) {
        backtrack_ms = (uint32_t)(backtrack_ms * (uint32_t)(s_app.reorient_retry_count + 1U) / 2U);
    }

    LF_Sensor_ReadFrame(&s_app.last_frame);

    if (time_reached(now_ms, s_app.reorient_backtrack_start_ms, backtrack_ms)) {
        s_app.reorient_backtrack_active = false;
        s_app.reorient_start_ms = now_ms;   /* 重置超时计时 */
        set_state(LF_APP_STATE_REORIENT_SPIN);
        LF_LedBlink_SetPattern(LF_LED_HEARTBEAT);
        LF_Platform_DebugPrint("Reorient: backtrack done, re-spin\n");
        return;
    }

    /* 倒车 */
    LF_Chassis_SetCommand((int16_t)(-backtrack_speed), (int16_t)(-backtrack_speed));
}

static void process_reorient_stop(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);
    LF_Chassis_Stop();

    /* 等待 reorient_stop_ms（debug profile 为 0，直角命中后立即旋转） */
    if (!time_reached(now_ms, s_app.reorient_start_ms, g_lf_config.reorient_stop_ms)) {
        return;
    }

    set_reorient_spin_command();
    s_app.reorient_start_ms = now_ms;  /* 重置超时计时，从旋转开始算 */
    set_state(LF_APP_STATE_REORIENT_SPIN);
    LF_Platform_DebugPrint("Reorient: stop wait done, spinning\n");
}

static void process_reorient_spin(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);

    /* 超时保护 → 倒车重试 */
    if ((uint32_t)(now_ms - s_app.reorient_start_ms) >= g_lf_config.reorient_timeout_ms) {
        reorient_retry_or_stop(now_ms);
        return;
    }

    /* 最短旋转时间未到时，不允许因为入口处中间通道仍亮而立刻确认完成。 */
    if ((uint32_t)(now_ms - s_app.reorient_start_ms) < g_lf_config.reorient_min_spin_ms) {
        set_reorient_spin_command();
        return;
    }

    /* 中间传感器看到线 → 进入确认 */
    if (middle_sensors_aligned(&s_app.last_frame)) {
        LF_Chassis_Stop();
        s_app.reorient_confirm_count = 1U;
        set_state(LF_APP_STATE_REORIENT_CONFIRM);
        LF_Platform_DebugPrint("Reorient: confirming\n");
        return;
    }

    /* 原地旋转：reorient_spin_dir > 0 → 右转(左轮正, 右轮负)
     *            reorient_spin_dir < 0 → 左转(左轮负, 右轮正) */
    set_reorient_spin_command();
}

static void process_reorient_confirm(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);

    /* 超时保护 → 倒车重试 */
    if ((uint32_t)(now_ms - s_app.reorient_start_ms) >= g_lf_config.reorient_timeout_ms) {
        reorient_retry_or_stop(now_ms);
        return;
    }

    if (middle_sensors_aligned(&s_app.last_frame)) {
        s_app.reorient_confirm_count += 1U;
        if (s_app.reorient_confirm_count >= g_lf_config.reorient_confirm_ticks) {
            /* 对准成功：重置 PID，恢复 RUNNING */
            LF_Control_ResetPid(&s_app.pid);
            s_app.current_target_speed = g_lf_config.min_speed;
            s_app.reorient_last_finish_ms = now_ms;  /* 记录冷却起点 */
            set_reason(LF_APP_REASON_REORIENT_ALIGNED);
            set_state(LF_APP_STATE_RUNNING);
            LF_Platform_DebugPrint("Reorient: aligned, resume\n");
            return;
        }
    } else {
        /* 中间传感器丢失线 → 回到旋转继续找 */
        s_app.reorient_confirm_count = 0U;
        set_state(LF_APP_STATE_REORIENT_SPIN);
        LF_Platform_DebugPrint("Reorient: lost, back to spin\n");
    }
    LF_Chassis_Stop();
}

static void process_running(uint32_t now_ms, float dt_s)
{
    int16_t correction;
    int16_t left_cmd;
    int16_t right_cmd;
    int16_t base_speed;
    float error;

    LF_Sensor_ReadFrame(&s_app.last_frame);

    /* 路段检测 + 连续弯方向追踪（在仲裁前执行，供 handle_line_lost 使用） */
    if (g_lf_config.segment_control_enable) {
        detect_segment_type();
        update_curve_direction_history();
    }

    LF_RunDecision decision = arbitrate_running_action(now_ms);

    switch (decision.action) {
    case LF_RUN_ACTION_LINE_LOST:
        handle_line_lost(now_ms);
        return;

    case LF_RUN_ACTION_FORK:
        start_fork_sample(now_ms);
        return;

    case LF_RUN_ACTION_OBSTACLE:
        reset_lead_phase();
        set_reason(LF_APP_REASON_RADAR_BLOCK);
        start_avoidance(now_ms);
        return;

    case LF_RUN_ACTION_LEAD_COMPENSATION:
        process_lead_phase();
        return;

    case LF_RUN_ACTION_EDGE_REALIGN:
        process_edge_realign();
        return;

    case LF_RUN_ACTION_CURVE_ARC:
        process_curve_arc();
        return;

    case LF_RUN_ACTION_REORIENT:
        start_reorient(now_ms);
        return;

    case LF_RUN_ACTION_PID_CONTROL:
#if TDPS_SIMPLE_CONTROL
        /* 简化 6 参数 PD+kff：连续速度函数，无死区/软区/积分/Kp 缩放 */
        {
            float ratio;
            int16_t raw_speed;
            float active_kp = g_lf_config.kp;
            float active_kd = g_lf_config.kd;
            float active_kff = g_lf_config.kff;
            int16_t active_base_speed = g_lf_config.base_speed;
            int16_t active_min_speed = g_lf_config.min_speed;
            int16_t active_max_correction = g_lf_config.max_correction;

            /* 分段参数选择（平滑过渡到目标参数集） */
            if (g_lf_config.segment_control_enable) {
                static LF_SegmentType blended_segment = LF_SEGMENT_STRAIGHT;
                static float blend_kp = 0.25f, blend_kd = 0.60f, blend_kff = 0.0f;
                static int16_t blend_bs = 100, blend_ms = 60;
                static int16_t blend_mc = 300;
                float target_kp, target_kd, target_kff;
                int16_t target_bs, target_ms, target_mc;
                const float blend_alpha = 0.5f;  /* 每帧过渡 50% */

                switch (s_app.segment_type) {
                case LF_SEGMENT_STRAIGHT:
                    target_kp = g_lf_config.seg_kp_straight;
                    target_kd = g_lf_config.seg_kd_straight;
                    target_kff = g_lf_config.seg_kff_straight;
                    target_bs = g_lf_config.seg_base_speed_straight;
                    target_ms = g_lf_config.seg_min_speed_straight;
                    target_mc = g_lf_config.seg_max_correction_straight;
                    break;
                case LF_SEGMENT_GENTLE_CURVE:
                    target_kp = g_lf_config.seg_kp_gentle_curve;
                    target_kd = g_lf_config.seg_kd_gentle_curve;
                    target_kff = g_lf_config.seg_kff_gentle_curve;
                    target_bs = g_lf_config.seg_base_speed_gentle_curve;
                    target_ms = g_lf_config.seg_min_speed_gentle_curve;
                    target_mc = g_lf_config.seg_max_correction_gentle_curve;
                    break;
                case LF_SEGMENT_TIGHT_CURVE:
                    target_kp = g_lf_config.seg_kp_tight_curve;
                    target_kd = g_lf_config.seg_kd_tight_curve;
                    target_kff = g_lf_config.seg_kff_tight_curve;
                    target_bs = g_lf_config.seg_base_speed_tight_curve;
                    target_ms = g_lf_config.seg_min_speed_tight_curve;
                    target_mc = g_lf_config.seg_max_correction_tight_curve;
                    break;
                case LF_SEGMENT_WIDE_LINE:
                    target_kp = g_lf_config.seg_kp_wide_line;
                    target_kd = g_lf_config.seg_kd_wide_line;
                    target_kff = g_lf_config.seg_kff_wide_line;
                    target_bs = g_lf_config.seg_base_speed_wide_line;
                    target_ms = g_lf_config.seg_min_speed_wide_line;
                    target_mc = g_lf_config.seg_max_correction_wide_line;
                    break;
                case LF_SEGMENT_FORK:
                    target_kp = g_lf_config.seg_kp_fork;
                    target_kd = g_lf_config.seg_kd_fork;
                    target_kff = g_lf_config.seg_kff_fork;
                    target_bs = g_lf_config.seg_base_speed_fork;
                    target_ms = g_lf_config.seg_min_speed_fork;
                    target_mc = g_lf_config.seg_max_correction_fork;
                    break;
                case LF_SEGMENT_LOST:
                default:
                    /* 丢线时回落全局默认参数 */
                    target_kp = g_lf_config.kp;
                    target_kd = g_lf_config.kd;
                    target_kff = g_lf_config.kff;
                    target_bs = g_lf_config.base_speed;
                    target_ms = g_lf_config.min_speed;
                    target_mc = g_lf_config.max_correction;
                    break;
                }

                if (s_app.segment_type != blended_segment) {
                    blended_segment = s_app.segment_type;
                }
                /* 一阶 IIR 平滑过渡 */
                blend_kp  = blend_kp  * blend_alpha + target_kp  * (1.0f - blend_alpha);
                blend_kd  = blend_kd  * blend_alpha + target_kd  * (1.0f - blend_alpha);
                blend_kff = blend_kff * blend_alpha + target_kff * (1.0f - blend_alpha);
                blend_bs  = (int16_t)((float)blend_bs * blend_alpha + (float)target_bs * (1.0f - blend_alpha));
                blend_ms  = (int16_t)((float)blend_ms * blend_alpha + (float)target_ms * (1.0f - blend_alpha));
                blend_mc  = (int16_t)((float)blend_mc * blend_alpha + (float)target_mc * (1.0f - blend_alpha));

                active_kp = blend_kp;
                active_kd = blend_kd;
                active_kff = blend_kff;
                active_base_speed = blend_bs;
                active_min_speed = blend_ms;
                active_max_correction = blend_mc;
            }

            error = (float)(decision.interference
                            ? s_app.last_trusted_position
                            : s_app.last_frame.position);

            /* 速度用原始 |position| 计算——不受 soft_zone 衰减，入弯时及时降速 */
            {
                float raw_abs = (error > 0.0f) ? error : -error;
                ratio = raw_abs / 1750.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                raw_speed = (int16_t)((float)active_base_speed -
                             (float)(active_base_speed - active_min_speed) * ratio);
                if (raw_speed < active_min_speed) raw_speed = active_min_speed;
            }

            error = shape_control_error(error);

            /* 一阶 IIR 平滑速度变化，alpha=0.4 */
            base_speed = (int16_t)(0.4f * (float)raw_speed + 0.6f * (float)s_app.current_target_speed);
            if (base_speed < active_min_speed) base_speed = active_min_speed;
            s_app.current_target_speed = base_speed;

            /* PD + kff 前馈：使用选中参数集 */
            correction = LF_Control_UpdatePDWithParams(error, dt_s, base_speed,
                                                        &s_app.pid, active_kp, active_kd,
                                                        active_kff, active_max_correction);
        }
        if (decision.start_guard_active) {
            base_speed = limit_degraded_speed(g_lf_config.start_straight_guard_speed);
            if (s_app.last_frame.active_count >= g_lf_config.start_straight_guard_active_count) {
                correction = 0;
                LF_Control_ResetPid(&s_app.pid);
            } else {
                correction = limit_speed_abs(correction, g_lf_config.start_straight_guard_max_correction);
            }
            s_app.current_target_speed = base_speed;
        }
        LF_Control_ComputeMotorCmd(base_speed, correction, &left_cmd, &right_cmd);
        left_cmd = limit_degraded_speed(left_cmd);
        right_cmd = limit_degraded_speed(right_cmd);
        update_trusted_direction(&s_app.last_frame, decision.interference);
        LF_Chassis_SetCommand(left_cmd, right_cmd);
#else
        /* 原始 6 级速度链 + 死区/软区 + Kp 缩放 + 积分 */
        base_speed = choose_running_speed(&s_app.last_frame);
        error = (float)(decision.interference
                        ? s_app.last_trusted_position
                        : s_app.last_frame.position);
        error = shape_control_error(error);
        {
            float saved_kp = g_lf_config.kp;
            float ratio = (float)base_speed / (float)g_lf_config.base_speed;
            if (ratio < 0.18f) ratio = 0.18f;
            g_lf_config.kp = saved_kp * ratio;
            correction = LF_Control_UpdatePid(error, dt_s, &s_app.pid);
            g_lf_config.kp = saved_kp;
        }
        if (decision.start_guard_active) {
            base_speed = limit_degraded_speed(g_lf_config.start_straight_guard_speed);
            if (s_app.last_frame.active_count >= g_lf_config.start_straight_guard_active_count) {
                correction = 0;
                LF_Control_ResetPid(&s_app.pid);
            } else {
                correction = limit_speed_abs(correction, g_lf_config.start_straight_guard_max_correction);
            }
            s_app.current_target_speed = base_speed;
        }
        LF_Control_ComputeMotorCmd(base_speed, correction, &left_cmd, &right_cmd);
        left_cmd = limit_degraded_speed(left_cmd);
        right_cmd = limit_degraded_speed(right_cmd);
        update_trusted_direction(&s_app.last_frame, decision.interference);
        LF_Chassis_SetCommand(left_cmd, right_cmd);
#endif
        return;

    default:
        return;
    }
}

static void stop_after_avoid_failure(void)
{
    LF_Chassis_Stop();
    set_reason(LF_APP_REASON_AVOID_FAILED);
    set_state(LF_APP_STATE_STOPPED);
    LF_Platform_DebugPrint("Obstacle avoid failed -> STOP\n");
}

static void retry_avoidance(uint32_t now_ms)
{
    if (s_app.avoid_attempts >= g_lf_config.obstacle_max_attempts) {
        stop_after_avoid_failure();
        return;
    }

    switch_avoid_dir();
    set_reason(LF_APP_REASON_AVOID_RETRY);
    enter_avoid_state(LF_APP_STATE_AVOID_PREP, now_ms);
}

static void process_avoid_prep(uint32_t now_ms)
{
    uint32_t hold_ms = g_lf_config.obstacle_stop_ms;

    if (hold_ms < g_lf_config.obstacle_confirm_ms) {
        hold_ms = g_lf_config.obstacle_confirm_ms;
    }

    LF_Chassis_Stop();

    if (s_app.obstacle_state != LF_RADAR_OBSTACLE_BLOCK) {
        s_app.avoid_confirm_count = 0U;
        if (avoid_elapsed(now_ms, g_lf_config.obstacle_confirm_ms)) {
            LF_Control_ResetPid(&s_app.pid);
            set_reason(LF_APP_REASON_RADAR_CLEAR);
            set_state(LF_APP_STATE_RUNNING);
            return;
        }
    } else if (s_app.avoid_confirm_count < g_lf_config.radar_debounce_frames) {
        s_app.avoid_confirm_count += 1U;
    }

    if (!avoid_elapsed(now_ms, hold_ms) ||
        s_app.avoid_confirm_count < g_lf_config.radar_debounce_frames) {
        return;
    }

    if (s_app.avoid_attempts >= g_lf_config.obstacle_max_attempts) {
        stop_after_avoid_failure();
        return;
    }

    s_app.avoid_attempts += 1U;
    set_reason(LF_APP_REASON_AVOID_STARTED);
    enter_avoid_state(LF_APP_STATE_AVOID_TURN_OUT, now_ms);
}

static void process_avoid_turn_out(uint32_t now_ms)
{
    int16_t turn = g_lf_config.obstacle_turn_speed;

    if (obstacle_emergency_stop()) {
        LF_Chassis_Stop();
        retry_avoidance(now_ms);
        return;
    }

    if (s_app.avoid_dir < 0) {
        LF_Chassis_SetCommand((int16_t)(-turn), turn);
    } else {
        LF_Chassis_SetCommand(turn, (int16_t)(-turn));
    }

    if ((avoid_elapsed(now_ms, g_lf_config.obstacle_turn_out_min_ms) && obstacle_is_clear_enough()) ||
        avoid_elapsed(now_ms, g_lf_config.obstacle_turn_out_ms)) {
        enter_avoid_state(LF_APP_STATE_AVOID_BYPASS, now_ms);
    }
}

static void process_avoid_bypass(uint32_t now_ms)
{
    int16_t inner = g_lf_config.obstacle_bypass_inner_speed;
    int16_t outer = g_lf_config.obstacle_bypass_outer_speed;

    if (obstacle_emergency_stop()) {
        LF_Chassis_Stop();
        retry_avoidance(now_ms);
        return;
    }

    if (s_app.avoid_dir < 0) {
        LF_Chassis_SetCommand(inner, outer);
    } else {
        LF_Chassis_SetCommand(outer, inner);
    }

    if ((avoid_elapsed(now_ms, g_lf_config.obstacle_bypass_min_ms) && obstacle_is_clear_enough()) ||
        avoid_elapsed(now_ms, g_lf_config.obstacle_bypass_ms)) {
        enter_avoid_state(LF_APP_STATE_AVOID_TURN_IN, now_ms);
    }
}

static void process_avoid_turn_in(uint32_t now_ms)
{
    int16_t turn = g_lf_config.obstacle_turn_speed;

    if (obstacle_emergency_stop()) {
        LF_Chassis_Stop();
        retry_avoidance(now_ms);
        return;
    }

    if (s_app.avoid_dir < 0) {
        LF_Chassis_SetCommand(turn, (int16_t)(-turn));
    } else {
        LF_Chassis_SetCommand((int16_t)(-turn), turn);
    }

    LF_Sensor_ReadFrame(&s_app.last_frame);
    if ((avoid_elapsed(now_ms, g_lf_config.obstacle_turn_in_min_ms) &&
         s_app.last_frame.signal_sum >= g_lf_config.obstacle_line_reacquire_min_sum) ||
        avoid_elapsed(now_ms, g_lf_config.obstacle_turn_in_ms)) {
        s_app.avoid_confirm_count = 0U;
        enter_avoid_state(LF_APP_STATE_AVOID_REACQUIRE, now_ms);
    }
}

static void process_avoid_reacquire(uint32_t now_ms)
{
    int16_t turn = g_lf_config.recover_turn_speed;

    if (obstacle_emergency_stop()) {
        LF_Chassis_Stop();
        retry_avoidance(now_ms);
        return;
    }

    if (confirm_line_frame(&s_app.avoid_confirm_count,
                           g_lf_config.obstacle_reacquire_confirm_ticks,
                           g_lf_config.obstacle_line_reacquire_min_sum,
                           g_lf_config.recover_confidence_min)) {
        complete_line_recovery(LF_APP_REASON_AVOID_COMPLETED);
        return;
    }

    if (s_app.avoid_dir < 0) {
        LF_Chassis_SetCommand((int16_t)(-turn), turn);
    } else {
        LF_Chassis_SetCommand(turn, (int16_t)(-turn));
    }

    if (avoid_elapsed(now_ms, g_lf_config.obstacle_reacquire_timeout_ms)) {
        if (s_app.avoid_attempts >= g_lf_config.obstacle_max_attempts) {
            s_app.recover_start_ms = now_ms;
            enter_recovery_phase(LF_RECOVER_SWEEP, now_ms);
            set_reason(LF_APP_REASON_LINE_LOST);
            set_state(LF_APP_STATE_RECOVERING);
            return;
        }
        retry_avoidance(now_ms);
    }
}

static void process_recovery(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - s_app.recover_start_ms;
    uint32_t phase_elapsed = now_ms - s_app.recover_phase_start_ms;
    uint8_t confirm_ticks = g_lf_config.recover_confirm_ticks;

    if (confirm_ticks == 0U) {
        confirm_ticks = 1U;
    }

    if (g_lf_config.obstacle_avoid_enable &&
        s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        set_reason(LF_APP_REASON_RADAR_BLOCK);
        start_avoidance(now_ms);
        return;
    }

    if (elapsed >= g_lf_config.recover_timeout_ms) {
        LF_Chassis_Stop();
        set_reason(LF_APP_REASON_RECOVERY_TIMEOUT);
        set_state(LF_APP_STATE_STOPPED);
        LF_Platform_DebugPrint("Recovery timeout -> STOP\n");
        return;
    }

    if (s_app.recover_phase == LF_RECOVER_BACKTRACK) {
        int16_t back = limit_degraded_speed(g_lf_config.recover_backtrack_speed);
        LF_Chassis_SetCommand((int16_t)(-back), (int16_t)(-back));
        if (confirm_line_frame(&s_app.recover_confirm_count,
                               confirm_ticks,
                               g_lf_config.line_detect_min_sum,
                               g_lf_config.recover_confidence_min)) {
            enter_recovery_phase(LF_RECOVER_CONFIRM, now_ms);
            return;
        }
        if (phase_elapsed >= g_lf_config.recover_backtrack_ms) {
            enter_recovery_phase(LF_RECOVER_SWEEP, now_ms);
        }
        return;
    }

    if (s_app.recover_phase == LF_RECOVER_CONFIRM) {
        LF_Chassis_Stop();
        if (confirm_line_frame(&s_app.recover_confirm_count,
                               confirm_ticks,
                               g_lf_config.line_detect_min_sum,
                               g_lf_config.recover_confidence_min)) {
            complete_line_recovery(LF_APP_REASON_LINE_RECOVERED);
            return;
        }
        enter_recovery_phase(LF_RECOVER_SWEEP, now_ms);
        return;
    }

    {
        int32_t span = g_lf_config.recover_sweep_max_speed - g_lf_config.recover_sweep_start_speed;
        int16_t turn;

        if (span < 0) {
            span = 0;
        }
        turn = (int16_t)(g_lf_config.recover_sweep_start_speed +
                         (int32_t)(phase_elapsed * (uint32_t)span) /
                         (int32_t)(g_lf_config.recover_timeout_ms == 0U ? 1U : g_lf_config.recover_timeout_ms));
        turn = limit_degraded_speed(turn);

        if (s_app.last_seen_dir < 0) {
            LF_Chassis_SetCommand((int16_t)(-turn), turn);
        } else {
            LF_Chassis_SetCommand(turn, (int16_t)(-turn));
        }
    }

    if (confirm_line_frame(&s_app.recover_confirm_count,
                           confirm_ticks,
                           g_lf_config.line_detect_min_sum,
                           g_lf_config.recover_confidence_min)) {
        enter_recovery_phase(LF_RECOVER_CONFIRM, now_ms);
    }
}

void LF_App_Init(void)
{
    uint32_t now = LF_Platform_GetMillis();

    LF_Radar_Init();
    LF_Sensor_Init();
    LF_Control_ResetPid(&s_app.pid);
    LF_Chassis_Init();

    s_app.boot_ms = now;
    s_app.last_step_us = LF_Platform_GetMicros();
    s_app.wait_start_line_since_ms = 0U;
    s_app.calib_start_ms = 0U;
    s_app.recover_start_ms = 0U;
    s_app.avoid_state_start_ms = 0U;
    s_app.last_seen_dir = +1;
    s_app.avoid_dir = +1;
    s_app.avoid_attempts = 0U;
    s_app.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
    s_app.obstacle_distance_mm = 0U;
    s_app.radar_parse_error_count = 0U;
    s_app.radar_has_target = false;
    s_app.radar_frame_valid = false;
    s_app.radar_target_state = 0U;
    s_app.radar_frame_type = LF_RADAR_FRAME_NONE;
    s_app.radar_frame_count = 0U;
    s_app.radar_last_update_ms = 0U;
    s_app.radar_frame_age_ms = 0U;
    s_app.reason = LF_APP_REASON_WAIT_START;
    s_app.calibration_ok = false;
    s_app.calibration_degraded = false;
    s_app.recover_phase = 0U;
    s_app.recover_confirm_count = 0U;
    s_app.recover_phase_start_ms = 0U;
    s_app.avoid_confirm_count = 0U;
    s_app.fork_state_start_ms = now;
    s_app.fork_detect_count = 0U;
    s_app.fork_block_count = 0U;
    s_app.fork_valid_sample_count = 0U;
    s_app.fork_last_sample_frame_count = 0U;
    s_app.fork_decision = 0;
    s_app.fork_min_distance_mm = UINT16_MAX;
    s_app.fork_radar_stale = false;
    s_app.fork_reacquire_count = 0U;
    s_app.run_finalized = false;
    s_app.recovery_count = 0U;
    s_app.line_lost_count = 0U;
    s_app.straight_stable_count = 0U;
    s_app.curve_prepare_count = 0U;
    s_app.interference_count = 0U;
    s_app.interference_hold_count = 0U;
    s_app.lead_phase = (uint8_t)LF_LEAD_PHASE_IDLE;
    s_app.lead_event_count = 0U;
    s_app.lead_phase_ticks = 0U;
    s_app.lead_entry_memory_count = 0U;
    s_app.lead_entry_dir = 0;
    s_app.straight_noise_count = 0U;
    s_app.start_straight_guard_phase = (uint8_t)LF_START_STRAIGHT_GUARD_OFF;
    s_app.start_straight_release_count = 0U;
    s_app.run_start_ms = 0U;
    s_app.edge_realign_count = 0U;
    s_app.edge_realign_side = 0;
    s_app.curve_arc_count = 0U;
    s_app.curve_arc_release_count = 0U;
    s_app.curve_arc_side = 0;
    s_app.last_strong_curve_side = 0;
    s_app.last_curve_position = 0;
    s_app.last_trusted_position = 0;
    s_app.trusted_line_dir = +1;
    s_app.trusted_line_valid = false;
    s_app.current_target_speed = g_lf_config.base_speed;

    /* 路段检测初始化 */
    s_app.segment_type = LF_SEGMENT_STRAIGHT;
    s_app.segment_candidate = LF_SEGMENT_STRAIGHT;
    s_app.segment_candidate_count = 0U;
    s_app.segment_hold_count = 0U;
    {
        uint8_t i;
        uint8_t hist_len = g_lf_config.segment_history_len;
        if (hist_len < 4U) hist_len = 4U;
        if (hist_len > 20U) hist_len = 20U;
        for (i = 0U; i < 20U; i++) {
            s_app.position_sign_history[i] = 0;
        }
    }
    s_app.sign_history_index = 0U;
    s_app.direction_switch_count = 0U;
    s_app.right_angle_detect_count = 0U;
    s_app.right_angle_side = 0;
    s_app.reorient_retry_count = 0U;
    s_app.reorient_retry_reverse = false;
    s_app.reorient_backtrack_start_ms = 0U;
    s_app.reorient_backtrack_active = false;
    s_app.reorient_last_finish_ms = 0U;

    set_state(LF_APP_STATE_WAIT_START);
}

void LF_App_RunStep(void)
{
    uint32_t now_us = LF_Platform_GetMicros();
    uint32_t now_ms = LF_Platform_GetMillis();
    float dt_s;

    if (!time_reached(now_us, s_app.last_step_us,
                      (uint32_t)g_lf_config.control_period_ms * 1000U)) {
        return;
    }

    dt_s = (float)(now_us - s_app.last_step_us) / 1000000.0f;
    s_app.last_step_us = now_us;
    refresh_radar_snapshot(now_ms);

    switch (s_app.state) {
        case LF_APP_STATE_WAIT_START:
            LF_Chassis_Stop();
            if (LF_Platform_IsStartButtonPressed() ||
                (g_lf_config.auto_start_delay_ms > 0U &&
                 (uint32_t)(now_ms - s_app.boot_ms) >= g_lf_config.auto_start_delay_ms) ||
                wait_start_line_ready(now_ms)) {
                start_calibration(now_ms);
            }
            break;

        case LF_APP_STATE_CALIBRATING:
            run_calibration_motion(now_ms);
            LF_Sensor_UpdateCalibration();

            if ((uint32_t)(now_ms - s_app.calib_start_ms) >= g_lf_config.calibration_duration_ms) {
                const LF_SensorCalibration *calib;

                LF_Chassis_Stop();
                LF_Sensor_EndCalibration();
                calib = LF_Sensor_GetCalibration();
                s_app.calibration_ok = calib->calibrated;
                s_app.calibration_degraded = (calib->status == LF_SENSOR_CAL_DEGRADED);
                LF_Hook_OnCalibrationComplete(s_app.calibration_ok);

                if (!s_app.calibration_ok) {
                    LF_Chassis_Stop();
                    LF_RunLog_SetCalQuality(0U);
                    set_reason(LF_APP_REASON_CALIBRATION_FAILED);
                    set_state(LF_APP_STATE_FAULT);
                    LF_Platform_DebugPrint("Calibration failed -> FAULT\n");
                    return;
                }

                LF_Control_ResetPid(&s_app.pid);
                start_straight_guard_begin(now_ms);
                LF_RunLog_SetCalQuality(s_app.calibration_degraded ? 1U : 2U);
                set_reason(s_app.calibration_degraded ?
                           LF_APP_REASON_CALIBRATION_DEGRADED :
                           LF_APP_REASON_CALIBRATION_DONE);
                set_state(LF_APP_STATE_RUNNING);
                LF_Platform_DebugPrint(s_app.calibration_degraded ?
                                       "Calibration degraded -> RUNNING\n" :
                                       "Calibration end -> RUNNING\n");
            }
            break;

        case LF_APP_STATE_RUNNING:
            process_running(now_ms, dt_s);
            break;

        case LF_APP_STATE_RECOVERING:
            process_recovery(now_ms);
            break;

        case LF_APP_STATE_AVOID_PREP:
            process_avoid_prep(now_ms);
            break;

        case LF_APP_STATE_AVOID_TURN_OUT:
            process_avoid_turn_out(now_ms);
            break;

        case LF_APP_STATE_AVOID_BYPASS:
            process_avoid_bypass(now_ms);
            break;

        case LF_APP_STATE_AVOID_TURN_IN:
            process_avoid_turn_in(now_ms);
            break;

        case LF_APP_STATE_AVOID_REACQUIRE:
            process_avoid_reacquire(now_ms);
            break;

        case LF_APP_STATE_FORK_SAMPLE:
            process_fork_sample(now_ms);
            break;

        case LF_APP_STATE_FORK_COMMIT_LEFT:
            process_fork_commit(now_ms, -1);
            break;

        case LF_APP_STATE_FORK_COMMIT_RIGHT:
            process_fork_commit(now_ms, +1);
            break;

        case LF_APP_STATE_FORK_REACQUIRE:
            process_fork_reacquire(now_ms);
            break;

        case LF_APP_STATE_REORIENT_STOP:
            if (s_app.reorient_backtrack_active) {
                process_reorient_backtrack(now_ms);
            } else {
                process_reorient_stop(now_ms);
            }
            break;

        case LF_APP_STATE_REORIENT_APPROACH:
            process_reorient_approach(now_ms);
            break;

        case LF_APP_STATE_REORIENT_SPIN:
            if (s_app.reorient_backtrack_active) {
                process_reorient_backtrack(now_ms);
            } else {
                process_reorient_spin(now_ms);
            }
            break;

        case LF_APP_STATE_REORIENT_CONFIRM:
            if (s_app.reorient_backtrack_active) {
                process_reorient_backtrack(now_ms);
            } else {
                process_reorient_confirm(now_ms);
            }
            break;

        case LF_APP_STATE_STOPPED:
            LF_Chassis_Stop();
            if (g_lf_config.sensor_fast_calibration && wait_start_line_ready(now_ms)) {
                s_app.run_finalized = false;
                s_app.line_lost_count = 0U;
                s_app.recovery_count = 0U;
                start_calibration(now_ms);
                break;
            }
            if (!s_app.run_finalized) {
                LF_RunLog_Finalize((uint8_t)LF_APP_STATE_STOPPED, (uint8_t)s_app.reason);
                LF_LedBlink_SetPostmortemCode(decode_stop_reason(s_app.reason),
                                              (uint8_t)(s_app.recovery_count > 15U ? 15U : s_app.recovery_count));
                s_app.run_finalized = true;
            }
            break;

        case LF_APP_STATE_BOOT:
        case LF_APP_STATE_FAULT:
        default:
            LF_Chassis_Stop();
            if (s_app.state != LF_APP_STATE_FAULT) {
                set_reason(LF_APP_REASON_FAULT_FALLBACK);
                set_state(LF_APP_STATE_FAULT);
            }
            if (!s_app.run_finalized) {
                LF_RunLog_Finalize((uint8_t)LF_APP_STATE_FAULT, (uint8_t)s_app.reason);
                LF_LedBlink_SetPostmortemCode(decode_stop_reason(s_app.reason),
                                              (uint8_t)(s_app.recovery_count > 15U ? 15U : s_app.recovery_count));
                s_app.run_finalized = true;
            }
            break;
    }

    LF_WatchDebug_UpdateApp(&s_app);
}

const LF_AppContext *LF_App_GetContext(void)
{
    return &s_app;
}

void LF_App_NotifyCheckpoint(uint32_t checkpoint_id)
{
    LF_Hook_OnReservedCheckpoint(checkpoint_id);
}
