#include "lf_app.h"

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

static int16_t choose_running_speed(const LF_SensorFrame *frame)
{
    int16_t speed = g_lf_config.base_speed;

    if (frame != NULL &&
        ((frame->position > g_lf_config.adaptive_error_threshold) ||
         (frame->position < (int32_t)(-g_lf_config.adaptive_error_threshold)))) {
        speed = g_lf_config.sharp_turn_speed;
    } else if (frame != NULL &&
               (frame->line_confidence < g_lf_config.adaptive_confidence_threshold ||
                frame->contrast_value < g_lf_config.line_detect_min_contrast)) {
        speed = g_lf_config.adaptive_slow_speed;
    }
    if (g_lf_config.obstacle_avoid_enable &&
        s_app.obstacle_state == LF_RADAR_OBSTACLE_WARN &&
        speed > g_lf_config.obstacle_warn_speed) {
        speed = g_lf_config.obstacle_warn_speed;
    }
    return limit_degraded_speed(speed);
}

static void hold_last_line_direction(void)
{
    int16_t forward = limit_degraded_speed(g_lf_config.line_hold_speed);
    int16_t turn = limit_degraded_speed(g_lf_config.line_hold_turn_speed);

    if (s_app.last_seen_dir < 0) {
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

    if (frame == NULL || !frame->line_detected || frame->signal_sum < g_lf_config.fork_detect_min_sum) {
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

static void process_running(uint32_t now_ms, float dt_s)
{
    int16_t correction;
    int16_t left_cmd;
    int16_t right_cmd;
    int16_t base_speed;
    bool fork_candidate;
    float error;

    LF_Sensor_ReadFrame(&s_app.last_frame);

    if (!s_app.last_frame.line_detected) {
        s_app.fork_detect_count = 0U;
        if (s_app.last_frame.edge_hint != 0) {
            s_app.last_seen_dir = s_app.last_frame.edge_hint;
        }
        if (s_app.line_lost_count < UINT8_MAX) {
            s_app.line_lost_count += 1U;
        }
        if (s_app.line_lost_count <= g_lf_config.line_lost_grace_ticks) {
            hold_last_line_direction();
            set_reason(LF_APP_REASON_LINE_LOST);
            return;
        }
        s_app.recover_start_ms = LF_Platform_GetMillis();
        enter_recovery_phase(LF_RECOVER_BACKTRACK, s_app.recover_start_ms);
        set_reason(LF_APP_REASON_LINE_LOST);
        set_state(LF_APP_STATE_RECOVERING);
        LF_Hook_OnLineLost();
        return;
    }
    s_app.line_lost_count = 0U;

    fork_candidate = g_lf_config.fork_enable &&
                     !fork_cooldown_active(now_ms) &&
                     frame_looks_like_fork(&s_app.last_frame);
    if (fork_candidate) {
        if (s_app.fork_detect_count < UINT8_MAX) {
            s_app.fork_detect_count += 1U;
        }
        if (s_app.fork_detect_count >= g_lf_config.fork_detect_confirm_ticks) {
            start_fork_sample(now_ms);
            return;
        }
    } else {
        s_app.fork_detect_count = 0U;
    }

    if (!fork_candidate && g_lf_config.obstacle_avoid_enable &&
        s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        set_reason(LF_APP_REASON_RADAR_BLOCK);
        start_avoidance(now_ms);
        return;
    }

    /*
     * position 左负右正，目标值是 0。
     * error > 0 代表需要向右修正，error < 0 代表需要向左修正。
     */
    base_speed = choose_running_speed(&s_app.last_frame);
    error = (float)(-s_app.last_frame.position);
    correction = LF_Control_UpdatePid(error, dt_s, &s_app.pid);
    LF_Control_ComputeMotorCmd(base_speed, correction, &left_cmd, &right_cmd);
    left_cmd = limit_degraded_speed(left_cmd);
    right_cmd = limit_degraded_speed(right_cmd);

    if (s_app.last_frame.edge_hint != 0) {
        s_app.last_seen_dir = s_app.last_frame.edge_hint;
    } else if (s_app.last_frame.position < 0) {
        s_app.last_seen_dir = -1;
    } else if (s_app.last_frame.position > 0) {
        s_app.last_seen_dir = +1;
    }

    LF_Chassis_SetCommand(left_cmd, right_cmd);
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
    s_app.last_step_ms = now;
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

    set_state(LF_APP_STATE_WAIT_START);
}

void LF_App_RunStep(void)
{
    uint32_t now_ms = LF_Platform_GetMillis();
    float dt_s;

    if (!time_reached(now_ms, s_app.last_step_ms, g_lf_config.control_period_ms)) {
        return;
    }

    dt_s = (float)(now_ms - s_app.last_step_ms) / 1000.0f;
    s_app.last_step_ms = now_ms;
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

        case LF_APP_STATE_STOPPED:
            LF_Chassis_Stop();
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
}

const LF_AppContext *LF_App_GetContext(void)
{
    return &s_app;
}

void LF_App_NotifyCheckpoint(uint32_t checkpoint_id)
{
    LF_Hook_OnReservedCheckpoint(checkpoint_id);
}
