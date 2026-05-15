#include "lf_app.h"

#include <stddef.h>
#include <stdio.h>

#include "lf_chassis.h"
#include "lf_config.h"
#include "lf_control.h"
#include "lf_future_hooks.h"
#include "lf_platform.h"
#include "lf_radar.h"
#include "lf_sensor.h"

static LF_AppContext s_app;

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
    s_app.state = state;
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
         (frame->position < (int32_t)(-g_lf_config.adaptive_error_threshold)) ||
         frame->line_confidence < g_lf_config.adaptive_confidence_threshold)) {
        speed = g_lf_config.adaptive_slow_speed;
    }
    if (s_app.obstacle_state == LF_RADAR_OBSTACLE_WARN && speed > g_lf_config.obstacle_warn_speed) {
        speed = g_lf_config.obstacle_warn_speed;
    }
    return limit_degraded_speed(speed);
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
    LF_Control_ResetPid(&s_app.pid);
    set_reason(reason);
    set_state(LF_APP_STATE_RUNNING);
    LF_Hook_OnLineRecovered();
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
    uint32_t elapsed = now_ms - s_app.calib_start_ms;
    uint32_t slot = elapsed / g_lf_config.calibration_switch_interval_ms;
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
    float error;

    if (s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        set_reason(LF_APP_REASON_RADAR_BLOCK);
        start_avoidance(now_ms);
        return;
    }

    LF_Sensor_ReadFrame(&s_app.last_frame);

    if (!s_app.last_frame.line_detected) {
        if (s_app.last_frame.edge_hint != 0) {
            s_app.last_seen_dir = s_app.last_frame.edge_hint;
        }
        s_app.recover_start_ms = LF_Platform_GetMillis();
        enter_recovery_phase(LF_RECOVER_BACKTRACK, s_app.recover_start_ms);
        set_reason(LF_APP_REASON_LINE_LOST);
        set_state(LF_APP_STATE_RECOVERING);
        LF_Hook_OnLineLost();
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

    if (s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
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

    set_state(LF_APP_STATE_WAIT_START);
    LF_Platform_SetStatusLed(false);
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
            if (LF_Platform_IsStartButtonPressed() ||
                ((uint32_t)(now_ms - s_app.boot_ms) >= g_lf_config.auto_start_delay_ms)) {
                LF_Sensor_StartCalibration();
                s_app.calib_start_ms = now_ms;
                set_reason(LF_APP_REASON_CALIBRATION_STARTED);
                set_state(LF_APP_STATE_CALIBRATING);
                LF_Hook_OnCalibrationBegin();
                LF_Platform_DebugPrint("Calibration start\n");
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
                    set_reason(LF_APP_REASON_CALIBRATION_FAILED);
                    set_state(LF_APP_STATE_FAULT);
                    LF_Platform_DebugPrint("Calibration failed -> FAULT\n");
                    return;
                }

                LF_Control_ResetPid(&s_app.pid);
                set_reason(s_app.calibration_degraded ?
                           LF_APP_REASON_CALIBRATION_DEGRADED :
                           LF_APP_REASON_CALIBRATION_DONE);
                set_state(LF_APP_STATE_RUNNING);
                LF_Platform_SetStatusLed(true);
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

        case LF_APP_STATE_STOPPED:
            LF_Chassis_Stop();
            break;

        case LF_APP_STATE_BOOT:
        case LF_APP_STATE_FAULT:
        default:
            LF_Chassis_Stop();
            if (s_app.state != LF_APP_STATE_FAULT) {
                set_reason(LF_APP_REASON_FAULT_FALLBACK);
            }
            set_state(LF_APP_STATE_FAULT);
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
