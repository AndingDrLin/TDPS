/**
 * @file lf_app.c
 * @brief 巡线应用层状态机
 *
 * 设计原则：
 * 1. 主循环不阻塞，所有动作都通过状态机和时间戳推进。
 * 2. 正常巡线只使用一套通用 PD+kff 控制逻辑。
 * 3. 复杂路口通过“路线脚本 + 固定 90° 转弯”处理，不再散落补丁式判断。
 * 4. 直角检测只有一套入口，避免多处规则互相抢占。
 */
#include "lf_app.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "lf_chassis.h"
#include "lf_config.h"
#include "lf_future_hooks.h"
#include "lf_led_blink.h"
#include "lf_platform.h"
#include "lf_radar.h"
#include "lf_sensor_uart.h"
#include "lf_watch_debug.h"

/* ==================== 内部状态 ==================== */

static LF_AppContext s_app;

static uint8_t s_digital_cache[LF_SENSOR_COUNT];
static bool s_digital_cache_valid;
static uint8_t s_route_pending_phase = 0xFFU;

/* ==================== 通用工具 ==================== */

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static bool time_reached(uint32_t now, uint32_t start, uint32_t duration)
{
    return (uint32_t)(now - start) >= duration;
}

static void bump_u8(uint8_t *value)
{
    if (value != 0 && *value < UINT8_MAX) {
        *value += 1U;
    }
}

static int16_t limit_degraded_speed(int16_t speed)
{
    int16_t limit;

    if (!s_app.calibration_degraded) {
        return speed;
    }

    limit = g_lf_config.sensor_degraded_max_speed;
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

static void set_state(LF_AppState state)
{
    if (s_app.state == state) {
        return;
    }

    s_app.state = state;

    switch (state) {
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
    case LF_APP_STATE_AVOID_BYPASS:
    case LF_APP_STATE_AVOID_REACQUIRE:
    case LF_APP_STATE_FIXED_TURN_STOP:
    case LF_APP_STATE_FIXED_TURN_SPIN:
    case LF_APP_STATE_FIXED_TURN_SETTLE:
    case LF_APP_STATE_REORIENT_STOP:
    case LF_APP_STATE_REORIENT_SPIN:
    case LF_APP_STATE_REORIENT_CONFIRM:
        LF_LedBlink_SetPattern(LF_LED_HEARTBEAT);
        break;
    case LF_APP_STATE_STOPPED:
    case LF_APP_STATE_FAULT:
    default:
        break;
    }
}

/* ==================== 传感器语义 ==================== */

static void refresh_digital_cache(void)
{
    s_digital_cache_valid = LF_SensorUart_GetDigitalFrame(s_digital_cache);
}

static bool lane_on(uint8_t index)
{
    if (index >= LF_SENSOR_COUNT) {
        return false;
    }

    if (s_digital_cache_valid) {
        return s_digital_cache[index] == 0U;  /* Yahboom D 帧：0=黑线=灯亮 */
    }

    return s_app.last_frame.instant_norm[index] >= g_lf_config.line_detect_min_peak;
}

static bool lane_off(uint8_t index)
{
    return !lane_on(index);
}

static uint8_t count_lanes_on(void)
{
    uint8_t i;
    uint8_t count = 0U;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        if (lane_on(i)) {
            count++;
        }
    }
    return count;
}

static uint8_t count_active_filtered(uint8_t first, uint8_t last)
{
    uint8_t i;
    uint8_t count = 0U;

    if (last >= LF_SENSOR_COUNT) {
        last = (uint8_t)(LF_SENSOR_COUNT - 1U);
    }
    for (i = first; i <= last; ++i) {
        if (s_app.last_frame.filtered_u16[i] >= g_lf_config.line_detect_min_peak) {
            count++;
        }
    }
    return count;
}

static bool frame_looks_like_all_on_cross(void)
{
    if (s_digital_cache_valid) {
        return count_lanes_on() >= 7U;
    }

    return s_app.last_frame.line_detected && s_app.last_frame.active_count >= 7U;
}

/**
 * @brief 统一直角检测
 *
 * 左直角：ch0~5 亮，ch7 灭。
 * 右直角：ch2~7 亮，ch0 灭。
 * D 帧有效时优先用 D 帧；否则用 instant_norm，避免中值/IIR 滤波造成灭灯滞后。
 */
static int8_t detect_right_angle_side(void)
{
    bool left_angle;
    bool right_angle;

    left_angle = lane_off(7U) &&
                 lane_on(0U) && lane_on(1U) && lane_on(2U) &&
                 lane_on(3U) && lane_on(4U) && lane_on(5U);

    right_angle = lane_off(0U) &&
                  lane_on(2U) && lane_on(3U) && lane_on(4U) &&
                  lane_on(5U) && lane_on(6U) && lane_on(7U);

    if (left_angle && !right_angle) {
        return -1;
    }
    if (right_angle && !left_angle) {
        return +1;
    }
    return 0;
}

static bool middle_sensors_aligned(void)
{
    return count_active_filtered(2U, 5U) >= 3U;
}

static bool line_confirmed(uint8_t *counter, uint8_t ticks)
{
    if (ticks == 0U) {
        ticks = 1U;
    }

    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();

    if (!s_app.last_frame.line_detected ||
        s_app.last_frame.signal_sum < g_lf_config.line_detect_min_sum) {
        if (counter != 0) {
            *counter = 0U;
        }
        return false;
    }

    if (counter == 0) {
        return true;
    }

    bump_u8(counter);
    return *counter >= ticks;
}

/* ==================== 路线脚本 ==================== */

static bool fixed_turn_cooldown_active(uint32_t now_ms)
{
    return g_lf_config.fixed_turn_cooldown_ms > 0U &&
           s_app.route_last_event_ms > 0U &&
           (uint32_t)(now_ms - s_app.route_last_event_ms) < g_lf_config.fixed_turn_cooldown_ms;
}

static void route_set_pending(uint8_t phase)
{
    s_route_pending_phase = phase;
}

static bool route_candidate_confirmed(bool candidate)
{
    uint8_t required = g_lf_config.route_event_confirm_ticks;

    if (required == 0U) {
        required = 1U;
    }
    if (!candidate) {
        s_app.route_event_confirm_count = 0U;
        return false;
    }

    bump_u8(&s_app.route_event_confirm_count);
    return s_app.route_event_confirm_count >= required;
}

static void route_clear_confirm(void)
{
    s_app.route_event_confirm_count = 0U;
}

static void route_commit_pending(void)
{
    if (s_route_pending_phase != 0xFFU) {
        s_app.route_phase = s_route_pending_phase;
        s_route_pending_phase = 0xFFU;
    }
    route_clear_confirm();
}

/**
 * @brief 路线脚本只“窥探”当前路口，不直接改阶段
 *
 * 返回 true 表示本帧路口已被脚本识别。
 * turn_dir: -1 左转，+1 右转，0 表示明确要求直行。
 */
static bool route_script_peek(int8_t *turn_dir, uint8_t *event)
{
    bool all_on;
    int8_t right_angle_side;

    if (turn_dir != 0) {
        *turn_dir = 0;
    }
    if (event != 0) {
        *event = (uint8_t)LF_ROUTE_EVENT_NONE;
    }
    s_route_pending_phase = 0xFFU;

    if (!g_lf_config.route_script_enable || !g_lf_config.fixed_turn_enable) {
        route_clear_confirm();
        return false;
    }
    if (g_lf_config.route_event_cooldown_ms > 0U &&
        s_app.route_last_event_ms > 0U &&
        (uint32_t)(LF_Platform_GetMillis() - s_app.route_last_event_ms) < g_lf_config.route_event_cooldown_ms) {
        route_clear_confirm();
        return false;
    }

    all_on = frame_looks_like_all_on_cross();
    right_angle_side = detect_right_angle_side();

    switch ((LF_RoutePhase)s_app.route_phase) {
    case LF_ROUTE_PHASE_WAIT_ARM:
    case LF_ROUTE_PHASE_T_RIGHT:
        if (!all_on) {
            route_clear_confirm();
            return false;
        }
        if (!route_candidate_confirmed(true)) {
            return true;  /* 候选路口已匹配，先抑制独立 fallback。 */
        }
        if (turn_dir != 0) *turn_dir = +1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_FIRST_T_RIGHT;
        route_set_pending((uint8_t)LF_ROUTE_PHASE_LEFT_1);
        return true;

    case LF_ROUTE_PHASE_LEFT_1:
        if (right_angle_side >= 0) {
            route_clear_confirm();
            return false;
        }
        if (!route_candidate_confirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = -1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_LEFT_RIGHT_ANGLE;
        route_set_pending((uint8_t)LF_ROUTE_PHASE_CROSS);
        s_app.route_cross_count = 0U;
        s_app.route_cross_armed = false;
        return true;

    case LF_ROUTE_PHASE_CROSS:
        if (!all_on) {
            s_app.route_cross_armed = true;
            route_clear_confirm();
            return false;
        }
        if (!s_app.route_cross_armed) {
            return true;  /* 仍在同一个十字上，明确直行且不重复计数。 */
        }
        if (!route_candidate_confirmed(true)) {
            return true;
        }
        route_clear_confirm();
        s_app.route_cross_armed = false;
        s_app.route_cross_count++;
        if (s_app.route_cross_count >= 2U) {
            s_app.route_phase = (uint8_t)LF_ROUTE_PHASE_LEFT_2;
        }
        if (turn_dir != 0) *turn_dir = 0;
        return true;

    case LF_ROUTE_PHASE_LEFT_2:
        if (right_angle_side >= 0) {
            route_clear_confirm();
            return false;
        }
        if (!route_candidate_confirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = -1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_LEFT_RIGHT_ANGLE;
        route_set_pending((uint8_t)LF_ROUTE_PHASE_RIGHT_1);
        return true;

    case LF_ROUTE_PHASE_RIGHT_1:
        if (right_angle_side <= 0) {
            route_clear_confirm();
            return false;
        }
        if (!route_candidate_confirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = +1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_RIGHT_RIGHT_ANGLE;
        route_set_pending((uint8_t)LF_ROUTE_PHASE_RIGHT_STRAIGHT);
        return true;

    case LF_ROUTE_PHASE_RIGHT_STRAIGHT:
        if (right_angle_side <= 0) {
            route_clear_confirm();
            return false;
        }
        if (!route_candidate_confirmed(true)) {
            return true;
        }
        route_clear_confirm();
        s_app.route_phase = (uint8_t)LF_ROUTE_PHASE_RIGHT_2;
        if (turn_dir != 0) *turn_dir = 0;
        return true;

    case LF_ROUTE_PHASE_RIGHT_2:
        if (right_angle_side <= 0) {
            route_clear_confirm();
            return false;
        }
        if (!route_candidate_confirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = +1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_RIGHT_RIGHT_ANGLE;
        route_set_pending((uint8_t)LF_ROUTE_PHASE_T_LEFT);
        return true;

    case LF_ROUTE_PHASE_T_LEFT:
        if (!all_on) {
            route_clear_confirm();
            return false;
        }
        if (!route_candidate_confirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = -1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_T_LEFT;
        route_set_pending((uint8_t)LF_ROUTE_PHASE_LEFT_3);
        return true;

    case LF_ROUTE_PHASE_LEFT_3:
        if (right_angle_side >= 0) {
            route_clear_confirm();
            return false;
        }
        if (!route_candidate_confirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = -1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_LEFT_RIGHT_ANGLE;
        route_set_pending((uint8_t)LF_ROUTE_PHASE_DONE);
        return true;

    case LF_ROUTE_PHASE_DONE:
    default:
        route_clear_confirm();
        return false;
    }
}

/* ==================== 分段控制 ==================== */

static void update_direction_history(void)
{
    int8_t sign = 0;
    int8_t prev;

    if (s_app.last_frame.position < 0) {
        sign = -1;
    } else if (s_app.last_frame.position > 0) {
        sign = +1;
    }

    if (s_app.sign_history_index >= 20U) {
        s_app.sign_history_index = 0U;
    }

    prev = s_app.position_sign_history[s_app.sign_history_index];
    s_app.position_sign_history[s_app.sign_history_index] = sign;
    s_app.sign_history_index++;
    if (s_app.sign_history_index >= 20U) {
        s_app.sign_history_index = 0U;
        if (s_app.direction_switch_count > 0U) {
            s_app.direction_switch_count--;
        }
    }

    if (sign != 0 && prev != 0 && sign != prev) {
        bump_u8(&s_app.direction_switch_count);
    }
}

static LF_SegmentType choose_segment_candidate(void)
{
    int32_t abs_pos;

    if (!s_app.last_frame.line_detected) {
        return LF_SEGMENT_LOST;
    }

    abs_pos = abs_i32(s_app.last_frame.position);

    if (frame_looks_like_all_on_cross() || s_app.last_frame.active_count >= 6U) {
        return LF_SEGMENT_WIDE_LINE;
    }
    if (detect_right_angle_side() != 0 || abs_pos >= 850 || s_app.last_frame.active_count >= 5U) {
        return LF_SEGMENT_TIGHT_CURVE;
    }
    if (abs_pos >= 300 || s_app.last_frame.edge_hint != 0) {
        return LF_SEGMENT_GENTLE_CURVE;
    }
    return LF_SEGMENT_STRAIGHT;
}

static void detect_segment_type(void)
{
    LF_SegmentType candidate;

    if (!g_lf_config.segment_control_enable) {
        s_app.segment_type = LF_SEGMENT_STRAIGHT;
        return;
    }

    candidate = choose_segment_candidate();
    if (candidate == LF_SEGMENT_LOST) {
        s_app.segment_type = candidate;
        s_app.segment_candidate = candidate;
        s_app.segment_candidate_count = 0U;
        return;
    }

    if (s_app.segment_hold_count > 0U) {
        s_app.segment_hold_count--;
        return;
    }

    if (candidate == s_app.segment_type) {
        s_app.segment_candidate_count = 0U;
        return;
    }

    if (candidate == s_app.segment_candidate) {
        bump_u8(&s_app.segment_candidate_count);
    } else {
        s_app.segment_candidate = candidate;
        s_app.segment_candidate_count = 1U;
    }

    if (s_app.segment_candidate_count >= g_lf_config.segment_confirm_ticks) {
        s_app.segment_type = candidate;
        s_app.segment_candidate_count = 0U;
        s_app.segment_hold_count = g_lf_config.segment_hold_ticks;
    }
}

/* ==================== 雷达快照与避障 ==================== */

static void refresh_radar_snapshot(uint32_t now_ms)
{
    const LF_RadarState *radar;

    if (!g_lf_config.radar_enable) {
        s_app.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
        s_app.obstacle_distance_mm = 0U;
        s_app.radar_has_target = false;
        s_app.radar_frame_valid = false;
        return;
    }

    LF_Radar_Tick(now_ms);
    radar = LF_Radar_GetState();
    s_app.obstacle_state = radar->obstacle_state;
    s_app.obstacle_distance_mm = radar->distance_mm;
    s_app.radar_has_target = radar->has_target;
    s_app.radar_frame_valid = radar->frame_valid;
    s_app.radar_frame_count = radar->frame_count;
    s_app.radar_last_update_ms = radar->last_update_ms;
    s_app.radar_frame_age_ms = radar->frame_valid ? (uint32_t)(now_ms - radar->last_update_ms) : 0U;
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

static void start_avoidance(uint32_t now_ms)
{
    LF_Chassis_Stop();
    s_app.avoid_dir = choose_avoid_dir();
    s_app.avoid_confirm_count = 0U;
    s_app.avoid_state_start_ms = now_ms;
    set_state(LF_APP_STATE_AVOID_PREP);
}

static void process_avoid_prep(uint32_t now_ms)
{
    LF_Chassis_Stop();

    if (s_app.obstacle_state != LF_RADAR_OBSTACLE_BLOCK) {
        LF_Control_ResetPid(&s_app.pid);
        set_state(LF_APP_STATE_RUNNING);
        return;
    }

    if (!time_reached(now_ms, s_app.avoid_state_start_ms, g_lf_config.obstacle_stop_ms)) {
        return;
    }

    s_app.avoid_state_start_ms = now_ms;
    set_state(LF_APP_STATE_AVOID_BYPASS);
}

static void process_avoid_bypass(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - s_app.avoid_state_start_ms;
    uint32_t third = g_lf_config.obstacle_bypass_ms / 3U;
    int16_t turn = g_lf_config.obstacle_turn_speed;
    int16_t speed = g_lf_config.obstacle_bypass_speed;

    if (third == 0U) {
        third = 1U;
    }

    if (elapsed < third) {
        if (s_app.avoid_dir < 0) {
            LF_Chassis_SetCommand((int16_t)(-turn), turn);
        } else {
            LF_Chassis_SetCommand(turn, (int16_t)(-turn));
        }
    } else if (elapsed < (third * 2U)) {
        if (s_app.avoid_dir < 0) {
            LF_Chassis_SetCommand((int16_t)(speed / 2), speed);
        } else {
            LF_Chassis_SetCommand(speed, (int16_t)(speed / 2));
        }
    } else if (elapsed < g_lf_config.obstacle_bypass_ms) {
        if (s_app.avoid_dir < 0) {
            LF_Chassis_SetCommand(turn, (int16_t)(-turn));
        } else {
            LF_Chassis_SetCommand((int16_t)(-turn), turn);
        }
    } else {
        s_app.avoid_state_start_ms = now_ms;
        s_app.avoid_confirm_count = 0U;
        set_state(LF_APP_STATE_AVOID_REACQUIRE);
    }
}

static void process_avoid_reacquire(uint32_t now_ms)
{
    int16_t turn = g_lf_config.recover_sweep_speed;

    if (line_confirmed(&s_app.avoid_confirm_count, 3U)) {
        LF_Control_ResetPid(&s_app.pid);
        set_state(LF_APP_STATE_RUNNING);
        return;
    }

    if (s_app.avoid_dir < 0) {
        LF_Chassis_SetCommand((int16_t)(-turn), turn);
    } else {
        LF_Chassis_SetCommand(turn, (int16_t)(-turn));
    }

    if (time_reached(now_ms, s_app.avoid_state_start_ms, g_lf_config.obstacle_reacquire_timeout_ms)) {
        s_app.recover_start_ms = now_ms;
        s_app.recover_phase_start_ms = now_ms;
        s_app.recover_phase = (uint8_t)LF_RECOVER_SWEEP_LEFT;
        s_app.recover_confirm_count = 0U;
        set_state(LF_APP_STATE_RECOVERING);
    }
}

/* ==================== 固定转弯与直角兜底 ==================== */

static uint16_t fixed_turn_duration(void)
{
    return (s_app.fixed_turn_dir < 0) ?
        g_lf_config.fixed_turn_90_ms_left :
        g_lf_config.fixed_turn_90_ms_right;
}

static void set_spin_command(int8_t dir, int16_t speed)
{
    if (dir > 0) {
        LF_Chassis_SetCommand(speed, (int16_t)(-speed));
    } else {
        LF_Chassis_SetCommand((int16_t)(-speed), speed);
    }
}

static void start_fixed_turn(uint32_t now_ms, int8_t dir, uint8_t event)
{
    if (!g_lf_config.fixed_turn_enable || dir == 0) {
        return;
    }

    route_commit_pending();
    LF_Control_ResetPid(&s_app.pid);
    s_app.fixed_turn_dir = dir;
    s_app.fixed_turn_event = event;
    s_app.fixed_turn_phase_start_ms = now_ms;

    if (g_lf_config.fixed_turn_stop_ms == 0U) {
        set_spin_command(s_app.fixed_turn_dir, g_lf_config.fixed_turn_spin_speed);
        set_state(LF_APP_STATE_FIXED_TURN_SPIN);
    } else {
        LF_Chassis_Stop();
        set_state(LF_APP_STATE_FIXED_TURN_STOP);
    }
}

static bool try_start_fixed_turn(uint32_t now_ms)
{
    int8_t dir = 0;
    uint8_t event = (uint8_t)LF_ROUTE_EVENT_NONE;

    if (!g_lf_config.fixed_turn_enable) {
        return false;
    }

    if (route_script_peek(&dir, &event)) {
        if (dir != 0) {
            start_fixed_turn(now_ms, dir, event);
            return true;
        }
        return false;  /* 路线脚本明确直行，禁止本帧兜底转弯。 */
    }

    if (!fixed_turn_cooldown_active(now_ms)) {
        dir = detect_right_angle_side();
        if (dir != 0 && abs_i32(s_app.last_frame.position) >= 300) {
            start_fixed_turn(now_ms, dir, (uint8_t)LF_ROUTE_EVENT_NONE);
            return true;
        }
    }

    return false;
}

static void process_fixed_turn_stop(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();
    LF_Chassis_Stop();

    if (time_reached(now_ms, s_app.fixed_turn_phase_start_ms, g_lf_config.fixed_turn_stop_ms)) {
        s_app.fixed_turn_phase_start_ms = now_ms;
        set_spin_command(s_app.fixed_turn_dir, g_lf_config.fixed_turn_spin_speed);
        set_state(LF_APP_STATE_FIXED_TURN_SPIN);
    }
}

static void process_fixed_turn_spin(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();

    if (!time_reached(now_ms, s_app.fixed_turn_phase_start_ms, fixed_turn_duration())) {
        set_spin_command(s_app.fixed_turn_dir, g_lf_config.fixed_turn_spin_speed);
        return;
    }

    LF_Chassis_Stop();
    s_app.fixed_turn_phase_start_ms = now_ms;
    set_state(LF_APP_STATE_FIXED_TURN_SETTLE);
}

static void process_fixed_turn_settle(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();
    LF_Chassis_Stop();

    if (!time_reached(now_ms, s_app.fixed_turn_phase_start_ms, g_lf_config.fixed_turn_settle_ms)) {
        return;
    }

    s_app.route_last_event_ms = now_ms;
    s_app.fixed_turn_dir = 0;
    s_app.fixed_turn_event = (uint8_t)LF_ROUTE_EVENT_NONE;
    s_app.current_target_speed = g_lf_config.min_speed;
    LF_Control_ResetPid(&s_app.pid);
    set_state(LF_APP_STATE_RUNNING);
}

static void start_reorient(uint32_t now_ms, int8_t dir)
{
    LF_Control_ResetPid(&s_app.pid);
    s_app.reorient_spin_dir = dir;
    s_app.reorient_start_ms = now_ms;
    s_app.reorient_confirm_count = 0U;
    LF_Chassis_Stop();
    set_state(LF_APP_STATE_REORIENT_STOP);
}

static void process_reorient_stop(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();
    LF_Chassis_Stop();

    /* 固定 60ms 停稳，避免入口处中间通道仍亮导致误确认。 */
    if (time_reached(now_ms, s_app.reorient_start_ms, 60U)) {
        s_app.reorient_start_ms = now_ms;
        set_spin_command(s_app.reorient_spin_dir, g_lf_config.reorient_spin_speed);
        set_state(LF_APP_STATE_REORIENT_SPIN);
    }
}

static void process_reorient_spin(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();

    if (time_reached(now_ms, s_app.reorient_start_ms, g_lf_config.reorient_timeout_ms)) {
        LF_Chassis_Stop();
        set_state(LF_APP_STATE_STOPPED);
        return;
    }

    if (middle_sensors_aligned()) {
        s_app.reorient_confirm_count = 1U;
        LF_Chassis_Stop();
        set_state(LF_APP_STATE_REORIENT_CONFIRM);
        return;
    }

    set_spin_command(s_app.reorient_spin_dir, g_lf_config.reorient_spin_speed);
}

static void process_reorient_confirm(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();
    LF_Chassis_Stop();

    if (!middle_sensors_aligned()) {
        set_state(LF_APP_STATE_REORIENT_SPIN);
        return;
    }

    bump_u8(&s_app.reorient_confirm_count);
    if (s_app.reorient_confirm_count >= g_lf_config.reorient_confirm_ticks) {
        s_app.reorient_last_finish_ms = now_ms;
        LF_Control_ResetPid(&s_app.pid);
        set_state(LF_APP_STATE_RUNNING);
    }
}

/* ==================== 巡线与恢复 ==================== */

static void update_trusted_direction(void)
{
    int8_t dir = 0;

    if (!s_app.last_frame.line_detected) {
        return;
    }

    if (s_app.last_frame.edge_hint != 0) {
        dir = s_app.last_frame.edge_hint;
    } else if (s_app.last_frame.position < 0) {
        dir = -1;
    } else if (s_app.last_frame.position > 0) {
        dir = +1;
    }

    if (dir != 0) {
        s_app.last_seen_dir = dir;
        s_app.trusted_line_dir = dir;
        s_app.last_trusted_position = s_app.last_frame.position;
        s_app.trusted_line_valid = true;
    }
}

static void hold_last_line_direction(void)
{
    int16_t forward = limit_degraded_speed((int16_t)(g_lf_config.min_speed));
    int16_t turn = limit_degraded_speed(g_lf_config.recover_sweep_speed);
    int8_t dir = s_app.trusted_line_valid ? s_app.trusted_line_dir : s_app.last_seen_dir;

    if (dir < 0) {
        LF_Chassis_SetCommand((int16_t)(forward - turn), (int16_t)(forward + turn));
    } else {
        LF_Chassis_SetCommand((int16_t)(forward + turn), (int16_t)(forward - turn));
    }
}

static void start_recovery(uint32_t now_ms)
{
    s_app.recover_start_ms = now_ms;
    s_app.recover_phase_start_ms = now_ms;
    s_app.recover_phase = (uint8_t)LF_RECOVER_FORWARD;
    s_app.recover_confirm_count = 0U;
    set_state(LF_APP_STATE_RECOVERING);
}

static void process_recovery(uint32_t now_ms)
{
    uint32_t phase_elapsed = now_ms - s_app.recover_phase_start_ms;
    int16_t speed;

    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();

    if (try_start_fixed_turn(now_ms)) {
        return;
    }

    if (line_confirmed(&s_app.recover_confirm_count, 3U)) {
        s_app.recovery_count++;
        LF_Control_ResetPid(&s_app.pid);
        s_app.line_lost_count = 0U;
        set_state(LF_APP_STATE_RUNNING);
        return;
    }

    if (time_reached(now_ms, s_app.recover_start_ms, g_lf_config.recover_timeout_ms)) {
        LF_Chassis_Stop();
        set_state(LF_APP_STATE_STOPPED);
        return;
    }

    switch ((LF_RecoverPhase)s_app.recover_phase) {
    case LF_RECOVER_FORWARD:
        speed = limit_degraded_speed(g_lf_config.recover_forward_speed);
        LF_Chassis_SetCommand(speed, speed);
        if (phase_elapsed >= g_lf_config.recover_forward_ms) {
            s_app.recover_phase = (uint8_t)LF_RECOVER_BACKTRACK;
            s_app.recover_phase_start_ms = now_ms;
        }
        break;

    case LF_RECOVER_BACKTRACK:
        speed = limit_degraded_speed(g_lf_config.recover_backtrack_speed);
        LF_Chassis_SetCommand((int16_t)(-speed), (int16_t)(-speed));
        if (phase_elapsed >= g_lf_config.recover_backtrack_ms) {
            s_app.recover_phase = (s_app.last_seen_dir < 0) ?
                (uint8_t)LF_RECOVER_SWEEP_LEFT : (uint8_t)LF_RECOVER_SWEEP_RIGHT;
            s_app.recover_phase_start_ms = now_ms;
        }
        break;

    case LF_RECOVER_SWEEP_LEFT:
        speed = limit_degraded_speed(g_lf_config.recover_sweep_speed);
        LF_Chassis_SetCommand((int16_t)(-speed), speed);
        if (phase_elapsed >= g_lf_config.recover_sweep_ms) {
            s_app.recover_phase = (uint8_t)LF_RECOVER_SWEEP_RIGHT;
            s_app.recover_phase_start_ms = now_ms;
        }
        break;

    case LF_RECOVER_SWEEP_RIGHT:
        speed = limit_degraded_speed(g_lf_config.recover_sweep_speed);
        LF_Chassis_SetCommand(speed, (int16_t)(-speed));
        if (phase_elapsed >= g_lf_config.recover_sweep_ms) {
            s_app.recover_phase = (uint8_t)LF_RECOVER_SWEEP_LEFT;
            s_app.recover_phase_start_ms = now_ms;
        }
        break;

    case LF_RECOVER_CONFIRM:
    default:
        LF_Chassis_Stop();
        break;
    }
}

static void run_pd_control(float dt_s)
{
    float error;
    float raw_abs;
    float ratio;
    int16_t raw_speed;
    int16_t base_speed;
    int16_t correction;
    int16_t left_cmd;
    int16_t right_cmd;
    float kp = g_lf_config.kp;
    float kd = g_lf_config.kd;
    float kff = g_lf_config.kff;
    int16_t target_base = g_lf_config.base_speed;
    int16_t target_min = g_lf_config.min_speed;
    int16_t target_max_corr = g_lf_config.max_correction;

    if (g_lf_config.segment_control_enable && s_app.segment_type != LF_SEGMENT_STRAIGHT) {
        kp = g_lf_config.seg_kp_curve;
        kd = g_lf_config.seg_kd_curve;
        kff = g_lf_config.seg_kff_curve;
        target_base = g_lf_config.seg_base_speed_curve;
        target_min = g_lf_config.seg_min_speed_curve;
        target_max_corr = g_lf_config.seg_max_correction_curve;
    } else if (g_lf_config.segment_control_enable) {
        kp = g_lf_config.seg_kp_straight;
        kd = g_lf_config.seg_kd_straight;
        kff = g_lf_config.seg_kff_straight;
        target_base = g_lf_config.seg_base_speed_straight;
        target_min = g_lf_config.seg_min_speed_straight;
        target_max_corr = g_lf_config.seg_max_correction_straight;
    }

    error = (float)s_app.last_frame.position;
    raw_abs = (error < 0.0f) ? -error : error;
    ratio = raw_abs / 1750.0f;
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }

    raw_speed = (int16_t)((float)target_base -
                          (float)(target_base - target_min) * ratio);
    if (raw_speed < target_min) {
        raw_speed = target_min;
    }

    /* 速度一阶平滑，避免弯道入口/出口速度突变。 */
    base_speed = (int16_t)(0.4f * (float)raw_speed + 0.6f * (float)s_app.current_target_speed);
    if (base_speed < target_min) {
        base_speed = target_min;
    }
    s_app.current_target_speed = base_speed;

    correction = LF_Control_UpdatePDWithParams(error,
                                               dt_s,
                                               base_speed,
                                               &s_app.pid,
                                               kp,
                                               kd,
                                               kff,
                                               target_max_corr);
    LF_Control_ComputeMotorCmd(base_speed, correction, &left_cmd, &right_cmd);
    LF_Chassis_SetCommand(limit_degraded_speed(left_cmd), limit_degraded_speed(right_cmd));
    update_trusted_direction();
}

static void process_running(uint32_t now_ms, float dt_s)
{
    int8_t right_angle_side;
    bool reorient_cooling;

    LF_Sensor_ReadFrame(&s_app.last_frame);
    refresh_digital_cache();
    detect_segment_type();
    update_direction_history();

    if (try_start_fixed_turn(now_ms)) {
        return;
    }

    if (!s_app.last_frame.line_detected) {
        bump_u8(&s_app.line_lost_count);
        if (s_app.line_lost_count <= g_lf_config.line_lost_grace_ticks) {
            hold_last_line_direction();
            return;
        }
        start_recovery(now_ms);
        return;
    }
    s_app.line_lost_count = 0U;

    if (g_lf_config.obstacle_avoid_enable && s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        start_avoidance(now_ms);
        return;
    }

    reorient_cooling = g_lf_config.reorient_cooldown_ms > 0U &&
        s_app.reorient_last_finish_ms > 0U &&
        (uint32_t)(now_ms - s_app.reorient_last_finish_ms) < g_lf_config.reorient_cooldown_ms;
    right_angle_side = detect_right_angle_side();
    if (g_lf_config.reorient_enable && !g_lf_config.fixed_turn_enable && !reorient_cooling &&
        right_angle_side != 0 && abs_i32(s_app.last_frame.position) >= g_lf_config.reorient_position_threshold) {
        start_reorient(now_ms, right_angle_side);
        return;
    }

    run_pd_control(dt_s);
}

/* ==================== 启动与标定 ==================== */

static bool wait_start_line_ready(uint32_t now_ms)
{
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
    return (uint32_t)(now_ms - s_app.wait_start_line_since_ms) >= g_lf_config.start_line_hold_ms;
}

static void start_calibration(uint32_t now_ms)
{
    LF_Sensor_StartCalibration();
    s_app.calib_start_ms = now_ms;
    s_app.wait_start_line_since_ms = 0U;
    set_state(LF_APP_STATE_CALIBRATING);
}

static void run_calibration_motion(uint32_t now_ms)
{
    uint32_t elapsed;
    uint32_t slot;
    int16_t spin;

    if (g_lf_config.sensor_fast_calibration) {
        LF_Chassis_Stop();
        return;
    }

    elapsed = now_ms - s_app.calib_start_ms;
    slot = elapsed / g_lf_config.calibration_switch_interval_ms;
    spin = g_lf_config.calibration_spin_speed;

    if ((slot & 0x1U) == 0U) {
        LF_Chassis_SetCommand(spin, (int16_t)(-spin));
    } else {
        LF_Chassis_SetCommand((int16_t)(-spin), spin);
    }
}

/* ==================== 公共 API ==================== */

void LF_App_Init(void)
{
    uint32_t now = LF_Platform_GetMillis();

    memset(&s_app, 0, sizeof(s_app));
    LF_Radar_Init();
    LF_Sensor_Init();
    LF_Control_ResetPid(&s_app.pid);
    LF_Chassis_Init();

    s_app.boot_ms = now;
    s_app.last_step_us = LF_Platform_GetMicros();
    s_app.last_seen_dir = +1;
    s_app.trusted_line_dir = +1;
    s_app.current_target_speed = g_lf_config.base_speed;
    s_app.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
    s_app.route_phase = (uint8_t)LF_ROUTE_PHASE_WAIT_ARM;
    s_app.route_cross_armed = true;
    s_app.segment_type = LF_SEGMENT_STRAIGHT;
    s_app.segment_candidate = LF_SEGMENT_STRAIGHT;
    s_app.fixed_turn_event = (uint8_t)LF_ROUTE_EVENT_NONE;
    s_route_pending_phase = 0xFFU;

    set_state(LF_APP_STATE_WAIT_START);
}

void LF_App_RunStep(void)
{
    uint32_t now_us = LF_Platform_GetMicros();
    uint32_t now_ms = LF_Platform_GetMillis();
    float dt_s;

    if (!time_reached(now_us,
                      s_app.last_step_us,
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
        if (time_reached(now_ms, s_app.calib_start_ms, g_lf_config.calibration_duration_ms)) {
            const LF_SensorCalibration *calib;
            LF_Chassis_Stop();
            LF_Sensor_EndCalibration();
            calib = LF_Sensor_GetCalibration();
            s_app.calibration_ok = calib->calibrated;
            s_app.calibration_degraded = (calib->status == LF_SENSOR_CAL_DEGRADED);
            LF_Hook_OnCalibrationComplete(s_app.calibration_ok);
            if (!s_app.calibration_ok) {
                set_state(LF_APP_STATE_FAULT);
                break;
            }
            s_app.run_start_ms = now_ms;
            s_app.current_target_speed = g_lf_config.base_speed;
            LF_Control_ResetPid(&s_app.pid);
            set_state(LF_APP_STATE_RUNNING);
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

    case LF_APP_STATE_AVOID_BYPASS:
        process_avoid_bypass(now_ms);
        break;

    case LF_APP_STATE_AVOID_REACQUIRE:
        process_avoid_reacquire(now_ms);
        break;

    case LF_APP_STATE_FIXED_TURN_STOP:
        process_fixed_turn_stop(now_ms);
        break;

    case LF_APP_STATE_FIXED_TURN_SPIN:
        process_fixed_turn_spin(now_ms);
        break;

    case LF_APP_STATE_FIXED_TURN_SETTLE:
        process_fixed_turn_settle(now_ms);
        break;

    case LF_APP_STATE_REORIENT_STOP:
        process_reorient_stop(now_ms);
        break;

    case LF_APP_STATE_REORIENT_SPIN:
        process_reorient_spin(now_ms);
        break;

    case LF_APP_STATE_REORIENT_CONFIRM:
        process_reorient_confirm(now_ms);
        break;

    case LF_APP_STATE_STOPPED:
        LF_Chassis_Stop();
        if (!s_app.run_finalized) {
            LF_LedBlink_SetPostmortemCode(1U, (uint8_t)(s_app.recovery_count > 15U ? 15U : s_app.recovery_count));
            s_app.run_finalized = true;
        }
        break;

    case LF_APP_STATE_BOOT:
    case LF_APP_STATE_FAULT:
    default:
        LF_Chassis_Stop();
        if (!s_app.run_finalized) {
            LF_LedBlink_SetPostmortemCode(3U, (uint8_t)(s_app.recovery_count > 15U ? 15U : s_app.recovery_count));
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
